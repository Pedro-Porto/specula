#pragma once
#include <string>
#include <chrono>
#include <vector>

#include "../../core/include/protocol.h"
#include "../../core/include/connection.h"

#include "server.h"
#include "stats_repo.h"

class Console {
public:
    Console(Server& server, StatsRepo& repo, CmdRepo& cmdRepo);

    int repl();



private:
    Server& server_;
    StatsRepo& statsRepo_;
    CmdRepo& cmdRepo_;

    void installSignalsOnce();
    void printStatus();
    void runStatus(bool watch, int interval_ms);
    void runExec(bool all, int id, std::string& cmd);
    void sleepFor(std::chrono::milliseconds ms);
};
