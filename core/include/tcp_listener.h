#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>

#include "tcp_socket.h"

// TcpListener class is used to create a TCP socket listener that can accept incoming connections.
class TcpListener : public TcpSocket {
   public:
    TcpListener() = default;  // Default constructor
    explicit TcpListener(uint16_t port,
                         const std::string& bindAddr = "0.0.0.0") {
        open(port, bindAddr);  // Initialize and open the listener
    }

    // open method is used to open the listener on the specified port and bind address.
    bool open(uint16_t port, const std::string& bindAddr = "0.0.0.0") {
        close();  // Close any existing socket
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);  // Create a new TCP socket
        if (fd_ < 0) return false;  // Return false if socket creation fails
        int yes = 1;
        // Allow address reuse
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};  // Address structure for binding
        addr.sin_family = AF_INET;  // Set address family to IPv4
        addr.sin_port = htons(port);  // Convert port to network byte order
        // Convert bind address from text to binary form, use any available address if conversion fails
        if (::inet_pton(AF_INET, bindAddr.c_str(), &addr.sin_addr) <= 0)
            addr.sin_addr.s_addr = INADDR_ANY;

        // Bind the socket to the address and start listening with a backlog of 16 connections
        if (::bind(fd_, (sockaddr*)&addr, sizeof(addr)) != 0) return false;
        if (::listen(fd_, 16) != 0) return false;
        return true;  // Successfully opened the listener
    }

    // accept method is used to accept a new incoming connection.
    TcpSocket accept() const {
        // Accept a new connection, throwing an exception if accept fails
        int cfd = ::accept(fd_, nullptr, nullptr);
        if (cfd < 0) throw std::runtime_error("accept failed");
        // Return a TcpSocket object for the accepted connection
        return TcpSocket(cfd);
    }
};
