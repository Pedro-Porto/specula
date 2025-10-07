#pragma once
#include "../../core/include/connection.h"
#include "../../core/include/protocol.h"
#include "stats_repo.h"
#include "cmd_repo.h"
#include <string>

/**
 * @class CommandRegistry
 * @brief The CommandRegistry class is responsible for registering and attaching commands
 *        to a connection, allowing the execution of various protocol commands.
 */
class CommandRegistry {
public:
    /**
     * @brief Construct a new Command Registry object
     * 
     * @param statsRepo Reference to the StatsRepo object for statistics management.
     * @param cmdRepo Reference to the CmdRepo object for command management.
     * @param token A string token used for authentication or identification purposes.
     */
    CommandRegistry(StatsRepo& statsRepo, CmdRepo& cmdRepo, const std::string& token);

    /**
     * @brief Attach the command registry to a connection.
     * 
     * This method registers all the necessary commands (e.g., Auth, Ping, Pong, Status, Bye, ExecOut, ExecDone)
     * to the given connection.
     * 
     * @param c Reference to the Connection object to which the commands will be attached.
     */
    void attach(Connection& c);

private:
    StatsRepo& statsRepo_; ///< Reference to the StatsRepo object.
    CmdRepo& cmdRepo_; ///< Reference to the CmdRepo object.
    std::string token_; ///< The token string.

    /**
     * @brief Register the Auth command for the connection.
     * 
     * @param c Reference to the Connection object.
     */
    void registerAuth_(Connection& c);

    /**
     * @brief Register the Ping command for the connection.
     * 
     * @param c Reference to the Connection object.
     */
    void registerPing_(Connection& c);

    /**
     * @brief Register the Pong command for the connection.
     * 
     * @param c Reference to the Connection object.
     */
    void registerPong_(Connection& c);

    /**
     * @brief Register the Status command for the connection.
     * 
     * @param c Reference to the Connection object.
     */
    void registerStatus_(Connection& c);

    /**
     * @brief Register the Bye command for the connection.
     * 
     * @param c Reference to the Connection object.
     */
    void registerBye_(Connection& c);

    /**
     * @brief Register the ExecOut command for the connection.
     * 
     * @param c Reference to the Connection object.
     */
    void registerExecOut_(Connection& c);

    /**
     * @brief Register the ExecDone command for the connection.
     * 
     * @param c Reference to the Connection object.
     */
    void registerExecDone_(Connection& c);

    /**
     * @brief Register a default command for the connection.
     * 
     * This method is called when no specific command matches the incoming request.
     * 
     * @param c Reference to the Connection object.
     */
    void registerDefault_(Connection& c);
};
