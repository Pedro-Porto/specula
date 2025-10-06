

#include <iomanip>
#include <iostream>

#include "../../core/include/utils.h"
#include "../core/include/connection.h"
#include "../core/include/tcp_client.h"
#include "../include/system_helpers.h"

int main() {
    const char* HOST = "127.0.0.1";
    const uint16_t PORT = 60119;
    const std::string TOKEN = "supersecret";

    TcpClient cli;
    if (!cli.connectTo(HOST, PORT)) {
        std::cerr << "[agent] connect failed\n";
        return 1;
    }
    std::cout << "[agent] connected, fd=" << cli.fd() << "\n";

    Connection conn(cli.release());

    conn.setDefaultHandler([](Connection&, const std::string& payload) {
        std::cerr << "[controller->agent][UNKNOWN] " << payload;
    });


    conn.on("PING",
            [](Connection& c, const std::string&) { c.send("PONG", ""); });

    conn.on("STATUS", [](Connection& c, const std::string&) {
        float cpu = get_cpu_percent();
        uint64_t mem_used_kb = 0, mem_total_kb = 0;
        get_mem(mem_used_kb, mem_total_kb);
        uint64_t dsk_used_kb = 0, dsk_total_kb = 0;
        get_disk(dsk_used_kb, dsk_total_kb, "/");

        std::ostringstream os;
        os << "cpu=" << std::fixed << std::setprecision(1) << cpu << "% "
           << "mem=" << mem_used_kb << "/" << mem_total_kb << " "
           << "disk=" << dsk_used_kb << "/" << dsk_total_kb << "\n";
        c.send("STATUS", os.str());
    });

    conn.on("EXEC", [](Connection& c, const std::string& payload) {
        std::cout << "[agent] EXEC received\n";
        auto nl = payload.find('\n');
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
            std::ostringstream os;
            os << "id=" << id << " code=" << code << "\n";
            c.send("EXEC_DONE", os.str());
        } else {
            int code = exec_command_stream(cmd, [&](const std::string& chunk) {
                std::ostringstream os;
                os << "id=" << id << "\n"
                   << chunk;
                c.send("EXEC_OUT", os.str());
            });
            std::ostringstream os;
            os << "id=" << id << " code=" << code << "\n";
            c.send("EXEC_DONE", os.str());
        }
    });

    std::atomic<bool> want_close{false};
    conn.on("BYE", [&](Connection& c, const std::string&) {
        c.send("OK", "bye\n");
        want_close =
            true; 
    });

    conn.start();
    conn.send("AUTH", TOKEN);

    while (conn.isRunning() && !want_close.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    conn.stop();
    std::cout << "[agent] shutdown\n";
    return 0;
}
