#include "../include/cli.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "../../core/include/utils.h"
#include "../include/cli_utils.h"

using namespace std::chrono;
namespace {
std::atomic<bool> g_stop{
    false};  // Global flag to signal when to stop execution
void on_sigint(int) {
    g_stop.store(true, std::memory_order_relaxed);
}  // Signal handler for SIGINT and SIGTERM
}  // namespace

void Console::installSignalsOnce() {
    static std::once_flag once;  // Ensure signals are installed only once
    std::call_once(once, [] {
        std::signal(SIGINT, on_sigint);   // Handle Ctrl+C
        std::signal(SIGTERM, on_sigint);  // Handle termination signal
    });
}

Console::Console(Server& server, StatsRepo& statsRepo, CmdRepo& cmdRepo)
    : server_(server), statsRepo_(statsRepo), cmdRepo_(cmdRepo) {
    installSignalsOnce();  // Install signal handlers during initialization
}

void Console::printStatus() {
    print_table(
        {"ID", "CPU%", "MEM (used/total)", "MEM%", "DISK (used/total)",
         "DSK%"},  // Table headers
        [&]() {
            std::vector<std::vector<std::string>> out;
            auto rows =
                statsRepo_.snapshot();  // Get a snapshot of current stats
            for (const auto& s : rows) {
                const auto memp = pct(
                    s.mem_used_bytes,
                    s.mem_total_bytes);  // Calculate memory usage percentage
                const auto dskp =
                    pct(s.disk_used_bytes,
                        s.disk_total_bytes);  // Calculate disk usage percentage

                std::ostringstream mem, dsk;
                mem << humanBytes(s.mem_used_bytes) << "/"
                    << humanBytes(s.mem_total_bytes);  // Format memory usage
                dsk << humanBytes(s.disk_used_bytes) << "/"
                    << humanBytes(s.disk_total_bytes);  // Format disk usage

                out.push_back(
                    {std::to_string(s.conn_id),
                     (std::ostringstream()
                      << std::fixed << std::setprecision(1) << s.cpu_percent)
                         .str(),  // Format CPU usage
                     mem.str(),
                     (std::ostringstream()
                      << std::fixed << std::setprecision(0) << memp)
                         .str(),  // Format memory percentage
                     dsk.str(),
                     (std::ostringstream()
                      << std::fixed << std::setprecision(0) << dskp)
                         .str()});  // Format disk percentage
            }
            return out;
        }(),
        "Status - watch (press Ctrl + C to stop)",  // Info message
        0                                           // Max width (0 means auto)
    );
}

void Console::runStatus(bool watch, int interval_ms) {
    g_stop.store(false);  // Reset the stop flag

    auto tick = [&] {
        server_.broadcast(std::string(specula::CMD_STATUS),
                          "");  // Request status from all agents
        sleepFor(std::chrono::milliseconds(150));  // Wait for responses
        printStatus();                             // Print the status table
    };

    if (!watch) {
        if (!g_stop.load()) tick();  // Run a single status update
        return;
    }

    while (!g_stop.load()) {
        tick();  // Periodically update status

        const int step = 25;  // Step interval for finer control
        int left = interval_ms;
        while (left > 0 && !g_stop.load()) {
            sleepFor(std::chrono::milliseconds(
                std::min(left, step)));  // Sleep in small steps
            left -= step;
        }
    }
    std::cout << std::endl;
}

void Console::sleepFor(std::chrono::milliseconds ms) {
    std::this_thread::sleep_for(ms);  // Sleep for the specified duration
}

void Console::runExec(bool all, int conn_id, std::string& cmd) {
    const bool monitor = !all;  // Monitor output only for specific connections

    auto wait_done_print = [&](int id, const std::string& prefix, bool follow) {
        using clk = std::chrono::steady_clock;
        auto start = clk::now();
        const auto timeout =
            std::chrono::seconds(60);  // Timeout for command execution
        std::string last_tail;

        while (true) {
            auto rec = cmdRepo_.get(id);  // Get the command record
            if (rec) {
                if (follow && rec->monitor) {
                    if (rec->tail != last_tail) {
                        // Print new output from the command
                        std::cout << "---- [" << prefix << " id=" << id
                                  << " stream] ----\n";
                        std::cout << rec->tail << std::flush;
                        last_tail = rec->tail;
                    }
                }
                if (rec->state == CmdRecord::State::Done) {
                    // Command execution completed
                    std::cout << "---- [" << prefix << " id=" << id << " done] "
                              << "exit_code=" << rec->exit_code
                              << " (bytes_out=" << rec->bytes_out
                              << ", chunks=" << rec->chunks_out << ")\n";
                    if (!follow && !rec->tail.empty()) {
                        std::cout << rec->tail << std::flush;
                    }
                    return;
                }
            }
            if (clk::now() - start > timeout) {
                // Timeout waiting for the result
                std::cout << "[" << prefix << " id=" << id
                          << "] timeout waiting result\n";
                return;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(150));  // Wait before retrying
        }
    };

    if (all) {
        std::vector<std::pair<int, int>> launched;
        server_.forEachConn([&](Connection& c) {
            if (!c.isRunning()) return;        // Skip inactive connections
            const int id = cmdRepo_.nextId();  // Generate a new command ID
            cmdRepo_.add(
                id, c.getCfd(), cmd,
                /*monitor=*/false);  // Add the command to the repository
            launched.push_back(std::make_pair(id, c.getCfd()));
        });

        for (auto& l : launched) {
            if (server_.send("EXEC",
                             "id=" + std::to_string(l.first) + " monitor=0\n" +
                                 cmd + "\n",
                             l.second)) {
                cmdRepo_.start(l.first);  // Mark the command as started
            } else {
                std::cout << "[exec] failed to send to conn_id=" << l.second
                          << "\n";
                cmdRepo_.erase(
                    l.first);  // Remove the command if sending failed
            }
        }

        if (launched.empty()) {
            std::cout << "no active connections\n";
            return;
        }

        for (auto& id : launched)
            wait_done_print(id.first, "all", /*follow=*/false);

        std::cout << "[exec] summary:\n";
        for (auto& id : launched) {
            auto r = cmdRepo_.get(id.first);
            if (!r) {
                std::cout << "  id=" << id.first << " no-result\n";
                continue;
            }
            std::cout << "  id=" << id.first << " conn=" << r->conn_id
                      << " code=" << r->exit_code << " out=" << r->bytes_out
                      << "B"
                      << " chunks=" << r->chunks_out << "\n";
        }
        return;
    }

    if (conn_id <= 0) {
        std::cout << "exec: invalid conn_id\n";
        return;
    }
    const int id = cmdRepo_.nextId();
    cmdRepo_.add(id, conn_id, cmd,
                 /*monitor=*/true);  // Add the command to the repository
    if (!server_.send("EXEC",
                      "id=" + std::to_string(id) + " monitor=" +
                          (monitor ? "1" : "0") + "\n" + cmd + "\n",
                      conn_id)) {
        std::cout << "[exec] failed to send to conn_id=" << conn_id << "\n";
        cmdRepo_.erase(id);  // Remove the command if sending failed
        return;
    }
    cmdRepo_.start(id);  // Mark the command as started
    std::cout << "[exec] launched id=" << id << " on conn_id=" << conn_id
              << " (monitor)\n";
    wait_done_print(id, "exec", /*follow=*/true);
}

int Console::repl() {
    std::cout << "Specula CLI — type 'help' for commands.\n";
    std::string line;

    while (true) {
        std::cout << "> " << std::flush;           // Print the prompt
        if (!std::getline(std::cin, line)) break;  // Read user input

        auto ltrim = [](std::string& s) {
            s.erase(s.begin(),
                    std::find_if(s.begin(), s.end(), [](unsigned char c) {
                        return !std::isspace(c);
                    }));  // Remove leading whitespace
        };
        auto rtrim = [](std::string& s) {
            s.erase(
                std::find_if(s.rbegin(), s.rend(),
                             [](unsigned char c) { return !std::isspace(c); })
                    .base(),
                s.end());  // Remove trailing whitespace
        };
        ltrim(line);
        rtrim(line);
        if (line.empty()) continue;  // Skip empty lines

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;  // Extract the command

        if (cmd == "help") {
            // Print help message
            std::cout
                << "Commands:\n"
                   "  status                           - request and print "
                   "current status from all agents\n"
                   "  status -w [ms]                   - watch mode; "
                   "refresh every [ms] (default 1500)\n"
                   "  exec <conn_id|all> <command...>  - execute command "
                   "on agent(s)\n"
                   "  ls                               - list active "
                   "connections\n"
                   "  clear                            - clear the screen\n"
                   "  quit | exit                      - leave the CLI\n";
            continue;
        }

        if (cmd == "quit" || cmd == "exit") {
            return 0;  // Exit the CLI
        }

        if (cmd == "clear") {
            std::cout << "\x1b[2J\x1b[H";  // Clear the screen
            continue;
        }

        if (cmd == "ls") {
            auto eps = server_.listEndpoints();
            if (eps.empty()) {
                std::cout << "no active connections\n";
            } else {
                // formata "ip:port", cuidando de IPv6 com colchetes
                auto fmt_addr = [](const std::string& ip, uint16_t port) {
                    std::ostringstream ss;
                    const bool ipv6 = ip.find(':') != std::string::npos;
                    if (ipv6)
                        ss << "[" << ip << "]:" << port;
                    else
                        ss << ip << ":" << port;
                    return ss.str();
                };

                std::vector<std::vector<std::string>> rows;
                rows.reserve(eps.size());
                for (const auto& [id, ep] : eps) {
                    rows.push_back({std::to_string(id),
                                    fmt_addr(ep.peer_ip, ep.peer_port),
                                    fmt_addr(ep.local_ip, ep.local_port)});
                }

                print_table({"ID", "Peer", "Local"}, rows, "Active connections",
                            0);
            }
            continue;
        }

        if (cmd == "exec") {
            std::string target;
            if (!(iss >> target)) {
                std::cout << "usage: exec <conn_id|all> <cmd>\n";
                continue;
            }

            std::string rest;
            std::getline(iss, rest);
            auto trim = [](std::string s) {
                auto issp = [](unsigned char c) { return std::isspace(c); };
                while (!s.empty() && issp(s.front())) s.erase(s.begin());
                while (!s.empty() && issp(s.back())) s.pop_back();
                return s;
            };
            rest = trim(rest);
            if (rest.empty()) {
                std::cout << "exec: missing command\n";
                continue;
            }

            if (target == "all") {
                runExec(/*all=*/true, /*conn_id=*/-1,
                        rest);  // Execute on all connections
            } else {
                int conn_id = -1;
                try {
                    conn_id = std::stoi(target);  // Parse connection ID
                } catch (...) {
                    conn_id = -1;
                }
                if (conn_id <= 0) {
                    std::cout << "exec: invalid target. use a numeric conn_id "
                                 "or 'all'\n";
                    continue;
                }
                runExec(/*all=*/false, conn_id,
                        rest);  // Execute on a specific connection
            }
            continue;
        }

        if (cmd == "status") {
            std::string opt;
            iss >> opt;

            if (opt == "-w") {
                int interval_ms = 1500;
                if (iss >> interval_ms) {
                    if (interval_ms < 100) interval_ms = 100;
                }
                runStatus(true, interval_ms);
            } else {
                runStatus(false, 0);
            }
            continue;
        }

        std::cout << "unknown command. type 'help'.\n";
    }
    return 0;
}
