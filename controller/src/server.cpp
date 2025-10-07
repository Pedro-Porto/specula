#include "../include/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <chrono>
#include <cstring>
#include <iostream>

// Server constructor that initializes the registry_ member variable
Server::Server(CommandRegistry& registry) : registry_(registry) {}

// Server destructor that stops the server
Server::~Server() { stop(); }

using std::optional;

static inline std::string ip_to_string(int family, const void* addr) {
    char buf[INET6_ADDRSTRLEN]{0};
    const char* p = inet_ntop(family, addr, buf, sizeof(buf));
    return p ? std::string(p) : std::string{};
}

optional<Server::Endpoint> Server::resolveEndpointFromFd(int fd) {
    sockaddr_storage peer{};
    socklen_t plen = sizeof(peer);
    if (getpeername(fd, reinterpret_cast<sockaddr*>(&peer), &plen) == -1) {
        return std::nullopt;
    }

    sockaddr_storage local{};
    socklen_t llen = sizeof(local);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&local), &llen) == -1) {
        return std::nullopt;
    }

    Endpoint ep{};
    ep.family = peer.ss_family;

    if (peer.ss_family == AF_INET) {
        auto* p4 = reinterpret_cast<sockaddr_in*>(&peer);
        auto* l4 = reinterpret_cast<sockaddr_in*>(&local);
        ep.peer_ip = ip_to_string(AF_INET, &p4->sin_addr);
        ep.peer_port = ntohs(p4->sin_port);
        ep.local_ip = ip_to_string(AF_INET, &l4->sin_addr);
        ep.local_port = ntohs(l4->sin_port);
    } else if (peer.ss_family == AF_INET6) {
        auto* p6 = reinterpret_cast<sockaddr_in6*>(&peer);
        auto* l6 = reinterpret_cast<sockaddr_in6*>(&local);
        ep.peer_ip = ip_to_string(AF_INET6, &p6->sin6_addr);
        ep.peer_port = ntohs(p6->sin6_port);
        ep.local_ip = ip_to_string(AF_INET6, &l6->sin6_addr);
        ep.local_port = ntohs(l6->sin6_port);
    } else {
        return std::nullopt;
    }

    return ep;
}

void Server::setEndpoint(int conn_id, Endpoint ep) {
    std::lock_guard<std::mutex> lk(ep_mtx_);
    endpoints_[conn_id] = std::move(ep);
}

void Server::eraseEndpoint(int conn_id) {
    std::lock_guard<std::mutex> lk(ep_mtx_);
    endpoints_.erase(conn_id);
}

std::optional<Server::Endpoint> Server::getEndpoint(int conn_id) const {
    std::lock_guard<std::mutex> lk(ep_mtx_);
    auto it = endpoints_.find(conn_id);
    if (it == endpoints_.end()) return std::nullopt;
    return it->second;
}

std::vector<std::pair<int, Server::Endpoint>> Server::listEndpoints() const {
    std::vector<std::pair<int, Server::Endpoint>> out;
    std::lock_guard<std::mutex> lk(ep_mtx_);
    out.reserve(endpoints_.size());
    for (auto& [id, ep] : endpoints_) out.emplace_back(id, ep);
    return out;
}

// Starts the server, binding to the given port and address
bool Server::start(uint16_t port, const std::string& bindAddr) {
    // Attempt to open the listener on the specified port and address
    if (!listener_.open(port, bindAddr)) {
        std::cerr << "[server] bind/listen failed on " << port << "\n";
        return false;
    }
    running_ = true;

    // Create a thread to accept incoming connections
    accept_thr_ = std::thread([this]() {
        while (running_.load()) {
            int cfd = -1;
            try {
                // Accept a new connection
                TcpSocket sock = listener_.accept();
                cfd = sock.release();  // Release the file descriptor from the
                                       // socket
            } catch (...) {
                // If the server is no longer running, exit the loop
                if (!running_.load()) break;
                continue;  // Otherwise, continue to the next iteration to
                           // accept new connections
            }

            // Create a shared pointer for the new connection
            auto conn = std::make_shared<Connection>(cfd);
            registry_.attach(*conn);  // Attach the connection to the registry
            conn->start();            // Start the connection
            conns_.add(conn);  // Add the connection to the connection manager
            if (auto ep = resolveEndpointFromFd(cfd)) {
                setEndpoint(cfd, *ep);
            }
        }
    });
    return true;
}

// Stops the server, closing the listener and joining the accept thread
void Server::stop() {
    // Exchange the running_ atomic flag to false, returning the previous value
    if (!running_.exchange(false)) return;

    listener_.close();  // Close the listener to stop accepting new connections

    // Join the accept thread if it is joinable
    if (accept_thr_.joinable()) accept_thr_.join();

    // Stop all active connections
    conns_.for_each([](auto& sp) {
        if (sp) sp->stop();
    });
}

// Broadcasts a message to all connected clients
void Server::broadcast(const std::string& cmd, const std::string& payload) {
    conns_.for_each([&](auto& sp) {
        // Send the message to each active connection
        if (sp && sp->isRunning()) {
            sp->send(cmd, payload);
        }
    });
}

// Sends a message to a specific client identified by conn_id
bool Server::send(const std::string& cmd, const std::string& payload,
                  int conn_id) {
    bool sent = false;
    conns_.for_each([&](auto& sp) {
        // Send the message if the connection is active and matches the conn_id
        if (sp && sp->isRunning() && sp->getCfd() == conn_id) {
            sent = sp->send(cmd, payload);
        }
    });
    return sent;
}
