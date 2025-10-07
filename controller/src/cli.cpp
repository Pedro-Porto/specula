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
std::atomic<bool> g_stop{false};
void on_sigint(int) { g_stop.store(true, std::memory_order_relaxed); }
}

void Console::installSignalsOnce() {
    static std::once_flag once;
    std::call_once(once, [] {
        std::signal(SIGINT, on_sigint);
        std::signal(SIGTERM, on_sigint);
    });
}

Console::Console(Server& server, StatsRepo& statsRepo, CmdRepo& cmdRepo)
    : server_(server), statsRepo_(statsRepo), cmdRepo_(cmdRepo) {
    installSignalsOnce();
}

void Console::printStatus() {
    print_table(
        {"ID", "CPU%", "MEM (used/total)", "MEM%", "DISK (used/total)", "DSK%"},
        [&]() {
            std::vector<std::vector<std::string>> out;
            auto rows = statsRepo_.snapshot();
            for (const auto& s : rows) {
                const auto memp = pct(s.mem_used_bytes, s.mem_total_bytes);
                const auto dskp = pct(s.disk_used_bytes, s.disk_total_bytes);

                std::ostringstream mem, dsk;
                mem << humanBytes(s.mem_used_bytes) << "/"
                    << humanBytes(s.mem_total_bytes);
                dsk << humanBytes(s.disk_used_bytes) << "/"
                    << humanBytes(s.disk_total_bytes);

                out.push_back(
                    {std::to_string(s.conn_id),
                     (std::ostringstream()
                      << std::fixed << std::setprecision(1) << s.cpu_percent)
                         .str(),
                     mem.str(),
                     (std::ostringstream()
                      << std::fixed << std::setprecision(0) << memp)
                         .str(),
                     dsk.str(),
                     (std::ostringstream()
                      << std::fixed << std::setprecision(0) << dskp)
                         .str()});
            }
            return out;
        }(),
        "Status - watch (press Ctrl + C to stop)",  // info
        0                                           // max_width
    );
}

void Console::runStatus(bool watch, int interval_ms) {
    g_stop.store(false);

    auto tick = [&] {
        server_.broadcast(std::string(specula::CMD_STATUS), "");
        sleepFor(std::chrono::milliseconds(150));
        printStatus();
    };

    if (!watch) {
        if (!g_stop.load()) tick();
        return;
    }

    while (!g_stop.load()) {
        tick();

        const int step = 25;
        int left = interval_ms;
        while (left > 0 && !g_stop.load()) {
            sleepFor(std::chrono::milliseconds(std::min(left, step)));
            left -= step;
        }
    }
    std::cout << std::endl;
}

void Console::sleepFor(std::chrono::milliseconds ms) {
    std::this_thread::sleep_for(ms);
}

void Console::runExec(bool all, int conn_id, std::string& cmd) {
    const bool monitor = !all;

    auto wait_done_print = [&](int id, const std::string& prefix, bool follow) {
        using clk = std::chrono::steady_clock;
        auto start = clk::now();
        const auto timeout = std::chrono::seconds(60);
        std::string last_tail;

        while (true) {
            auto rec = cmdRepo_.get(id);
            if (rec) {
                if (follow && rec->monitor) {
                    if (rec->tail != last_tail) {
                        std::cout << "---- [" << prefix << " id=" << id
                                  << " stream] ----\n";
                        std::cout << rec->tail << std::flush;
                        last_tail = rec->tail;
                    }
                }
                if (rec->state == CmdRecord::State::Done) {
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
                std::cout << "[" << prefix << " id=" << id
                          << "] timeout waiting result\n";
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    };

    if (all) {
        std::vector<std::pair<int, int>> launched;
        server_.forEachConn([&](Connection& c) {
            if (!c.isRunning()) return;
            const int id = cmdRepo_.nextId();
            cmdRepo_.add(id, c.getCfd(), cmd, /*monitor=*/false);
            launched.push_back(std::make_pair(id, c.getCfd()));
        });

        for (auto& l: launched) {
            if(server_.send("EXEC",
                      "id=" + std::to_string(l.first) + " monitor=0\n" + cmd + "\n",
                      l.second)){
                        cmdRepo_.start(l.first);
                      }
                      else {
                        std::cout << "[exec] failed to send to conn_id=" << l.second << "\n";
                        cmdRepo_.erase(l.first);

                      }
        }

        if (launched.empty()) {
            std::cout << "no active connections\n";
            return;
        }

        for (auto& id : launched) wait_done_print(id.first, "all", /*follow=*/false);

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
    cmdRepo_.add(id, conn_id, cmd, /*monitor=*/true);
    if (!server_.send("EXEC",
                      "id=" + std::to_string(id) + " monitor=" +
                          (monitor ? "1" : "0") + "\n" + cmd + "\n",
                      conn_id)) {
        std::cout << "[exec] failed to send to conn_id=" << conn_id << "\n";
        cmdRepo_.erase(id);
        return;    }
    cmdRepo_.start(id);
    std::cout << "[exec] launched id=" << id << " on conn_id=" << conn_id
              << " (monitor)\n";
    wait_done_print(id, "exec", /*follow=*/true);
}

int Console::repl() {
    std::cout << "Specula CLI — type 'help' for commands.\n";
    std::string line;

    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;

        auto ltrim = [](std::string& s) {
            s.erase(s.begin(),
                    std::find_if(s.begin(), s.end(), [](unsigned char c) {
                        return !std::isspace(c);
                    }));
        };
        auto rtrim = [](std::string& s) {
            s.erase(
                std::find_if(s.rbegin(), s.rend(),
                             [](unsigned char c) { return !std::isspace(c); })
                    .base(),
                s.end());
        };
        ltrim(line);
        rtrim(line);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "help") {
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
            return 0;
        }

        if (cmd == "clear") {
            std::cout << "\x1b[2J\x1b[H";
            continue;
        }

        if (cmd == "ls") {
            auto rows = statsRepo_.snapshot();
            if (rows.empty()) {
                std::cout << "no active connections\n";
            } else {
                std::cout << "Active connections (IDs): ";
                for (const auto& s : rows) {
                    std::cout << s.conn_id << " ";
                }
                std::cout << "\n";
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
                runExec(/*all=*/true, /*conn_id=*/-1, rest);
            } else {
                int conn_id = -1;
                try {
                    conn_id = std::stoi(target);
                } catch (...) {
                    conn_id = -1;
                }
                if (conn_id <= 0) {
                    std::cout << "exec: invalid target. use a numeric conn_id "
                                 "or 'all'\n";
                    continue;
                }
                runExec(/*all=*/false, conn_id, rest);
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
