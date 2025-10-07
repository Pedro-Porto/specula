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
    // Set up signal handler for SIGINT to request shutdown
    std::signal(SIGINT, on_sigint);

    const uint16_t PORT = 60119; // Port number for the server
    const std::string TOKEN = "supersecret"; // Authentication token for commands

    // Initialize repositories and registry
    StatsRepo statsRepo;
    CmdRepo cmdRepo;
    CommandRegistry registry(statsRepo, cmdRepo, TOKEN);
    Server server(registry);

    // Start the server and check for errors
    if (!server.start(PORT)) {
        std::cerr << "failed to start server\n";
        return 1;
    }

    // Set up a scheduler to broadcast a ping command every 2 seconds
    Scheduler sched;
    sched.every(std::chrono::seconds(2), [&] {
        server.broadcast(std::string(specula::CMD_PING), "");
    });

    std::cout << "[controller] running; press Ctrl-C to stop\n";

    // Start the command-line interface (CLI) for user interaction
    Console cli(server, statsRepo, cmdRepo);
    int rc = cli.repl();

    // Stop the scheduler and server during shutdown
    sched.stop();
    server.stop();
    std::cout << "[controller] shutdown\n";
    return 0;
}
