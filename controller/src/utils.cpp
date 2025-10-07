#include "../include/utils.h"

/**
 * @brief Broadcasts a command and its payload to all authenticated and running connections.
 *
 * This function iterates over all connections in the provided ThreadSafeVector and sends the
 * specified command and payload to each connection that is authenticated and currently running.
 *
 * @param cmd The command to broadcast.
 * @param payload The payload to send along with the command.
 * @param conns A thread-safe vector containing the connections to broadcast to.
 */
void broadcast(const std::string& cmd, const std::string& payload,
               ThreadSafeVector<Connection>& conns) {
    // Iterate over each connection in the thread-safe vector
    conns.for_each([&](auto& c) {
        // Check if the connection is valid, authenticated, and running
        if (c && c->isAuthenticated && c->isRunning()) {
            // Send the command and payload to the connection
            c->send(cmd, payload);
        }
    });
}