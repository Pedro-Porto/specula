#pragma once
#include "../../core/include/tcp_listener.h"
#include "../../core/include/tcp_socket.h"
#include "../../core/include/connection.h"
#include "command_registry.h"
#include "../include/thread_safe_vector.h"

#include <atomic>
#include <memory>
#include <thread>

class Server {
public:
    explicit Server(CommandRegistry& registry);
    ~Server();

    bool start(uint16_t port, const std::string& bindAddr = "0.0.0.0");
    void stop();

    void broadcast(const std::string& cmd, const std::string& payload);
    bool send(const std::string& cmd, const std::string& payload, int conn_id);

    template<typename F>
    void forEachConn(F&& f){
        conns_.for_each([&](auto& sp){
            if (sp && sp->isRunning()) f(*sp);
        });
    }

private:
    CommandRegistry& registry_;
    TcpListener listener_;
    std::atomic<bool> running_{false};
    std::thread accept_thr_;
    ThreadSafeVector<Connection> conns_;
    int next_id_{1};
};
