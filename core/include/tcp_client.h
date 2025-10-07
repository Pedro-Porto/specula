#pragma once
#include "tcp_socket.h"
#include <netdb.h>
#include <string>
#include <future>

class TcpClient : public TcpSocket {
public:
    TcpClient() = default; // Default constructor

    // Connect to the given host and port with an optional timeout
    bool connectTo(const std::string& host, uint16_t port, int timeoutMs = 5000) {
        close(); // Close any existing socket
        struct addrinfo hints{}, *res = nullptr; // Address info structures
        hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM; // Use TCP sockets

        std::string portStr = std::to_string(port); // Convert port to string
        // Resolve host and port, return false on failure
        if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) return false;

        bool ok = false; // Connection status
        // Iterate through address results
        for (auto p = res; p; p = p->ai_next) {
            fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol); // Create a socket
            if (fd_ < 0) continue; // Skip if socket creation fails

            struct timeval tv{}; // Timeout structure
            tv.tv_sec = timeoutMs / 1000; // Set timeout seconds
            tv.tv_usec = (timeoutMs % 1000) * 1000; // Set timeout microseconds
            // Set receive and send timeouts
            setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            // Attempt to connect, break on success
            if (::connect(fd_, p->ai_addr, p->ai_addrlen) == 0) { ok = true; break; }
            ::close(fd_); fd_ = -1; // Close socket on failure
        }
        freeaddrinfo(res); // Free address info resources
        return ok; // Return connection status
    }
};
