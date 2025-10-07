

#include <iomanip>
#include <iostream>
#include <chrono>
#include <thread>

#include "../../core/include/utils.h"
#include "../core/include/connection.h"
#include "../core/include/tcp_client.h"
#include "../include/system_helpers.h"

bool connectWithRetry(const char* host, uint16_t port, const std::string& token, std::unique_ptr<Connection>& conn) {
    const int MAX_RETRY_DELAY = 30; // max 30 seconds between retries
    int retry_delay = 1; // start with 1 second
// Attempts to connect to the server with exponential backoff on failure
    
    while (true) {
        std::cout << "[agent] attempting to connect to " << host << ":" << port << "\n";
        
        TcpClient cli;
        if (cli.connectTo(host, port)) {
            std::cout << "[agent] connected, fd=" << cli.fd() << "\n";
            
            conn = std::make_unique<Connection>(cli.release());
            conn->start();
            conn->send("AUTH", token);
            
            return true;
        }
        
        std::cerr << "[agent] connect failed, retrying in " << retry_delay << " seconds\n";
        std::this_thread::sleep_for(std::chrono::seconds(retry_delay));
        
        // Exponential backoff with cap
        retry_delay = std::min(retry_delay * 2, MAX_RETRY_DELAY);
    }
}

int main() {
    const char* HOST = "127.0.0.1";
    const uint16_t PORT = 60119;
    const std::string TOKEN = "supersecret";

    std::unique_ptr<Connection> conn;

    std::atomic<bool> want_close{false};
    
    auto setupHandlers = [&](Connection& c) {
        c.setDefaultHandler([](Connection&, const std::string& payload) {
            std::cerr << "[controller->agent][UNKNOWN] " << payload;
    // Lambda to set up message handlers for the connection
        });

        c.on("PING",
                [](Connection& c, const std::string&) { c.send("PONG", ""); });

        // Responds to PING with PONG
        c.on("STATUS", [](Connection& c, const std::string&) {
            float cpu = get_cpu_percent();
            uint64_t mem_used_kb = 0, mem_total_kb = 0;
        // Handles STATUS requests: sends CPU, memory, and disk usage
            get_mem(mem_used_kb, mem_total_kb);
            uint64_t dsk_used_kb = 0, dsk_total_kb = 0;
            get_disk(dsk_used_kb, dsk_total_kb, "/");

            std::ostringstream os;
            os << "cpu=" << std::fixed << std::setprecision(1) << cpu << "% "
               << "mem=" << mem_used_kb << "/" << mem_total_kb << " "
               << "disk=" << dsk_used_kb << "/" << dsk_total_kb << "\n";
            c.send("STATUS", os.str());
        });

        c.on("EXEC", [](Connection& c, const std::string& payload) {
            std::cout << "[agent] EXEC received\n";
            auto nl = payload.find('\n');
        // Handles EXEC requests: executes system commands
            std::string opts =
                (nl == std::string::npos) ? payload : payload.substr(0, nl);
            std::string cmd =
                (nl == std::string::npos) ? "" : payload.substr(nl + 1);

            auto kv = parse_kv(opts);
            int id = 0;
            bool monitor = false;
            try {
                if (kv.count("id")) id = std::stoi(kv["id"]);
                if (kv.count("monitor"))
                    monitor = (kv["monitor"] == "1" || kv["monitor"] == "true");
            } catch (...) {
            }

            cmd = trim(cmd);

            if (cmd.empty()) {
                c.send("EXEC_DONE",
                       "id=" + std::to_string(id) + " code=127\n");
                return;
            }

            if (!monitor) {
                int code = exec_command_stream(
                    cmd, [&](const std::string&) { });
            // If not monitoring, just execute and return exit code
                std::ostringstream os;
                os << "id=" << id << " code=" << code << "\n";
                c.send("EXEC_DONE", os.str());
            } else {
                int code = exec_command_stream(cmd, [&](const std::string& chunk) {
                    std::ostringstream os;
                    os << "id=" << id << "\n"
                // If monitoring, stream output chunks as EXEC_OUT
                       << chunk;
                    c.send("EXEC_OUT", os.str());
                });
                std::ostringstream os;
                os << "id=" << id << " code=" << code << "\n";
                c.send("EXEC_DONE", os.str());
            }
        });

        c.on("BYE", [&](Connection& c, const std::string&) {
            c.send("OK", "bye\n");
            want_close = true; 
        // Handles BYE requests: signals agent to close connection
        });
    };

    // Main connection loop with automatic reconnection
    while (!want_close.load()) {
        // Try to connect (with retries built-in)
    // Main connection loop: reconnects if connection is lost
        if (!connectWithRetry(HOST, PORT, TOKEN, conn)) {
            std::cerr << "[agent] failed to establish connection\n";
            continue;
        }
        
        // Setup message handlers
        setupHandlers(*conn);
        
        std::cout << "[agent] connection established, entering main loop\n";
        
        // Main message processing loop
        while (conn->isRunning() && !want_close.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Connection lost or want_close requested
        if (conn) {
            conn->stop();
            conn.reset();
        }
        
        if (!want_close.load()) {
            std::cerr << "[agent] connection lost, will attempt to reconnect\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    std::cout << "[agent] shutdown\n";
    return 0;
    // Agent shutdown
}
