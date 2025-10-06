#include <csignal>
#include <iostream>

#include "../core/include/protocol.h"
#include "./include/command_registry.h"
#include "./include/scheduler.h"
#include "./include/server.h"
#include "./include/shutdown.h"
#include "./include/stats_repo.h"
#include "./include/cli.h"

static Shutdown g_shutdown;
static void on_sigint(int) { g_shutdown.request(); }

int main() {
    std::signal(SIGINT, on_sigint);

    const uint16_t PORT = 60119;
    const std::string TOKEN = "supersecret";

    StatsRepo statsRepo;
    CmdRepo cmdRepo;
    CommandRegistry registry(statsRepo, cmdRepo, TOKEN);
    Server server(registry);

    if (!server.start(PORT)) {
        std::cerr << "failed to start server\n";
        return 1;
    }

    Scheduler sched;
    sched.every(std::chrono::seconds(2), [&] {
        server.broadcast(std::string(specula::CMD_PING), "");
    });

    std::cout << "[controller] running; press Ctrl-C to stop\n";
    Console cli(server, statsRepo, cmdRepo);
    int rc = cli.repl();

    sched.stop();
    server.stop();
    std::cout << "[controller] shutdown\n";
    return 0;
}
