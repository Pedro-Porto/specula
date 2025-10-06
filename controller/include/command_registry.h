#pragma once
#include "../../core/include/connection.h"
#include "../../core/include/protocol.h"
#include "stats_repo.h"
#include "cmd_repo.h"
#include <string>

class CommandRegistry {
public:
    CommandRegistry(StatsRepo& statsRepo, CmdRepo& cmdRepo, const std::string& token);
    void attach(Connection& c);

private:
    StatsRepo& statsRepo_;
    CmdRepo& cmdRepo_;
    std::string token_;

    void registerAuth_(Connection& c);
    void registerPing_(Connection& c);
    void registerPong_(Connection& c);
    void registerStatus_(Connection& c);
    void registerBye_(Connection& c);
    void registerExecOut_(Connection& c);
    void registerExecDone_(Connection& c);
    void registerDefault_(Connection& c);
};
