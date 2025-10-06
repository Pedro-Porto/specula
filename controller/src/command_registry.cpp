#include "../include/command_registry.h"

#include "../../core/include/utils.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

CommandRegistry::CommandRegistry(StatsRepo& statsRepo, CmdRepo& cmdRepo, const std::string& token)
    : statsRepo_(statsRepo), cmdRepo_(cmdRepo), token_(token) {}

void CommandRegistry::attach(Connection& c) {
    registerAuth_(c);
    registerPing_(c);
    registerPong_(c);
    registerExecOut_(c);
    registerExecDone_(c);
    registerStatus_(c);
    registerBye_(c);
    registerDefault_(c);
}

void CommandRegistry::registerAuth_(Connection& c) {
    c.on(std::string(specula::CMD_AUTH),
         [this](Connection& conn, const std::string& payload) {
             if (payload == token_) {
                 conn.isAuthenticated = true;
                 conn.send(std::string(specula::RESP_OK), "agent\n");
             } else {
                 conn.isAuthenticated = false;
                 conn.send(std::string(specula::RESP_ERR), "unauthorized\n");
             }
         });
}

void CommandRegistry::registerPing_(Connection& c) {
    c.on(std::string(specula::CMD_PING),
         [](Connection& conn, const std::string&) { conn.send("PONG", ""); });
}

void CommandRegistry::registerPong_(Connection& c) {
    c.on("PONG", [](Connection&, const std::string&) {
    });
}

void CommandRegistry::registerStatus_(Connection& c) {
    c.on(std::string(specula::CMD_STATUS), [this](Connection& conn,
                                                  const std::string& payload) {
        if (!conn.isAuthenticated) {
            conn.send(std::string(specula::RESP_ERR), "unauthorized\n");
            return;
        }

        auto kv = parse_kv(payload);
        Stats s{};
        s.conn_id = conn.getCfd();
        if (kv.find("cpu") != kv.end()) {
            std::string cpu_str = kv["cpu"];
            if (cpu_str.back() == '%') cpu_str.pop_back();
            s.cpu_percent = std::stof(cpu_str);
        }
        if (kv.find("mem") != kv.end()) {
            std::string mem_str = kv["mem"];
            auto slash = mem_str.find('/');
            if (slash != std::string::npos) {
                s.mem_used_bytes =
                    std::stoull(mem_str.substr(0, slash)) * 1024;  // assume KB
                s.mem_total_bytes =
                    std::stoull(mem_str.substr(slash + 1)) * 1024;
            }
        }
        if (kv.find("disk") != kv.end()) {
            std::string disk_str = kv["disk"];
            auto slash = disk_str.find('/');
            if (slash != std::string::npos) {
                s.disk_used_bytes =
                    std::stoull(disk_str.substr(0, slash)) * 1024;  // assume KB
                s.disk_total_bytes =
                    std::stoull(disk_str.substr(slash + 1)) * 1024;
            }
        }
        statsRepo_.upsert(s);
    });
}

void CommandRegistry::registerBye_(Connection& c) {
    c.on(std::string(specula::CMD_BYE),
         [](Connection& conn, const std::string&) {
             conn.send("OK", "bye\n");
         });
}

void CommandRegistry::registerExecOut_(Connection& c) {
    c.on("EXEC_OUT", [this](Connection& conn, const std::string& payload) {
        if (!conn.isAuthenticated) {
            conn.send(std::string(specula::RESP_ERR), "unauthorized\n");
            return;
        }
        auto nl = payload.find('\n');
        std::string opts =
            (nl == std::string::npos) ? payload : payload.substr(0, nl);
        std::string chunk =
            (nl == std::string::npos) ? "" : payload.substr(nl + 1);

        auto kv = parse_kv(opts);
        int id = 0;
        try {
            if (kv.count("id")) id = std::stoi(kv["id"]);
        } catch (...) {
        }

        if (id <= 0 || chunk.empty()) {
            return;
        }

        if (!cmdRepo_.appendOut(id, chunk)) {
            conn.send(std::string(specula::RESP_ERR), "invalid_id\n");
            return;
        }
    });
}

void CommandRegistry::registerExecDone_(Connection& c) {
    c.on("EXEC_DONE", [this](Connection& conn, const std::string& payload) {
        if (!conn.isAuthenticated) {
            conn.send(std::string(specula::RESP_ERR), "unauthorized\n");
            return;
        }
        auto kv = parse_kv(payload);
        int id = 0;
        int code = -1;
        try {
            if (kv.count("id")) id = std::stoi(kv["id"]);
            if (kv.count("code")) code = std::stoi(kv["code"]);
        } catch (...) {
        }

        if (id <= 0 || code < 0) {
            return;
        }

        if (!cmdRepo_.done(id, code)) {
            conn.send(std::string(specula::RESP_ERR), "invalid_id\n");
            return;
        }
    });
}

void CommandRegistry::registerDefault_(Connection& c) {
    c.setDefaultHandler([](Connection& conn, const std::string& payload) {
        std::cerr << "[command_registry] unknown command: payload='" << payload
                  << "'\n";
        conn.send(std::string(specula::RESP_ERR), "unknown_cmd\n");
    });
}
