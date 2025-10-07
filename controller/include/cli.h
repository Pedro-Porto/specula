#pragma once
#include <string>
#include <chrono>
#include <vector>

#include "../../core/include/protocol.h"
#include "../../core/include/connection.h"

#include "server.h"
#include "stats_repo.h"

/**
 * @class Console
 * @brief The Console class provides a command-line interface for interacting with the server.
 *
 * This class is responsible for handling user inputs, executing commands,
 * and displaying the server status and statistics.
 */
class Console {
public:
    /**
     * @brief Construct a new Console object.
     * 
     * @param server Reference to the Server object.
     * @param repo Reference to the StatsRepo object.
     * @param cmdRepo Reference to the CmdRepo object.
     */
    Console(Server& server, StatsRepo& repo, CmdRepo& cmdRepo);

    /**
     * @brief Read-eval-print loop for the console.
     * 
     * This method starts the REPL, processing user commands until
     * an exit command is received.
     * 
     * @return int Exit status code.
     */
    int repl();



private:
    Server& server_; ///< Reference to the Server object.
    StatsRepo& statsRepo_; ///< Reference to the StatsRepo object.
    CmdRepo& cmdRepo_; ///< Reference to the CmdRepo object.

    /**
     * @brief Install signal handlers for the console.
     * 
     * This method sets up any necessary signal handlers for
     * managing console behavior, such as cleanup on exit.
     */
    void installSignalsOnce();

    /**
     * @brief Print the current status of the server.
     * 
     * This method displays the server's status, including
     * uptime, connections, and other relevant statistics.
     */
    void printStatus();

    /**
     * @brief Run the status command at regular intervals.
     * 
     * This method repeatedly executes the status command
     * at the specified interval, updating the display.
     * 
     * @param watch If true, the status command will be run continuously.
     * @param interval_ms The interval between command executions, in milliseconds.
     */
    void runStatus(bool watch, int interval_ms);

    /**
     * @brief Execute a command on the server.
     * 
     * This method sends the specified command to the server
     * for execution, optionally targeting all connections or
     * a specific connection by ID.
     * 
     * @param all If true, the command is sent to all connections.
     * @param id The ID of the specific connection (ignored if all is true).
     * @param cmd The command to be executed.
     */
    void runExec(bool all, int id, std::string& cmd);

    /**
     * @brief Sleep for the specified duration.
     * 
     * This method pauses the execution for the given duration,
     * allowing for timed intervals between operations.
     * 
     * @param ms The duration to sleep, in milliseconds.
     */
    void sleepFor(std::chrono::milliseconds ms);
};
