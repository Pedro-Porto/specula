#pragma once
#include "../../core/include/tcp_listener.h"
#include "../../core/include/tcp_socket.h"
#include "../../core/include/connection.h"
#include "command_registry.h"
#include "../include/thread_safe_vector.h"

#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <optional>
#include <string>
#include <cstdint>

/**
 * @file server.h
 * @brief Server for managing client connections and broadcasting messages.
 */

/**
 * @class Server
 * @brief Manages client connections and facilitates message broadcasting.
 */
class Server {
public:
    /**
     * @brief Estrutura com endpoints da conexão (remoto e local).
     */
    struct Endpoint {
        std::string peer_ip;     ///< IP remoto (string canonical)
        uint16_t    peer_port{}; ///< Porta remota (host order)
        std::string local_ip;    ///< IP local (string canonical)
        uint16_t    local_port{};///< Porta local (host order)
        int         family{};    ///< AF_INET ou AF_INET6
    };

    /**
     * @brief Constructor for Server class.
     *
     * @param registry Command registry for managing commands.
     */
    explicit Server(CommandRegistry& registry);

    /**
     * @brief Destructor for Server class.
     */
    ~Server();

    /**
     * @brief Starts the server on the specified port.
     *
     * @param port The port number to bind the server to.
     * @param bindAddr The address to bind the server to (default is "0.0.0.0").
     * @return True if the server started successfully, false otherwise.
     */
    bool start(uint16_t port, const std::string& bindAddr = "0.0.0.0");

    /**
     * @brief Stops the server and disconnects all clients.
     */
    void stop();

    /**
     * @brief Broadcasts a message to all connected clients.
     *
     * @param cmd The command to broadcast.
     * @param payload The payload to send along with the command.
     */
    void broadcast(const std::string& cmd, const std::string& payload);

    /**
     * @brief Sends a message to a specific client.
     *
     * @param cmd The command to send.
     * @param payload The payload to send along with the command.
     * @param conn_id The connection ID of the recipient client.
     * @return True if the message was sent successfully, false otherwise.
     */
    bool send(const std::string& cmd, const std::string& payload, int conn_id);

    /**
     * @brief Iterates over all active connections and applies a function to each.
     *
     * @tparam F The type of the function to apply.
     * @param f The function to apply to each connection.
     */
    template<typename F>
    void forEachConn(F&& f){
        conns_.for_each([&](auto& sp){
            if (sp && sp->isRunning()) f(*sp);
        });
    }

    // -------------------- NOVO: APIs de endpoint --------------------

    /**
     * @brief Obtém o endpoint (peer/local) de uma conexão específica.
     * @param conn_id ID da conexão.
     * @return Endpoint caso encontrado; std::nullopt se não existir.
     */
    std::optional<Endpoint> getEndpoint(int conn_id) const;

    /**
     * @brief Lista endpoints de todas as conexões ativas (apenas as que estão rodando).
     */
    std::vector<std::pair<int, Endpoint>> listEndpoints() const;

private:
    // Preenche Endpoint a partir de um fd (chamado no accept).
    static std::optional<Endpoint> resolveEndpointFromFd(int fd);

    // Registra/atualiza o endpoint (chamado após aceitar uma conexão).
    void setEndpoint(int conn_id, Endpoint ep);

    // Remove endpoint do mapa (em stop() e quando a conexão encerrar).
    void eraseEndpoint(int conn_id);

private:
    CommandRegistry& registry_; ///< Command registry for managing commands.
    TcpListener listener_; ///< TCP listener for accepting client connections.
    std::atomic<bool> running_{false}; ///< Flag indicating whether the server is running.
    std::thread accept_thr_; ///< Thread for accepting incoming connections.
    ThreadSafeVector<Connection> conns_; ///< Thread-safe vector of active client connections.
    int next_id_{1}; ///< ID to be assigned to the next new connection.

    // ---- NOVO: índice conn_id -> Endpoint ----
    mutable std::mutex ep_mtx_;
    std::unordered_map<int, Endpoint> endpoints_;
};
