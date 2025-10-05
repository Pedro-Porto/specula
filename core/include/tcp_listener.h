#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>

#include "tcp_socket.h"

class TcpListener : public TcpSocket {
   public:
    TcpListener() = default;
    explicit TcpListener(uint16_t port,
                         const std::string& bindAddr = "0.0.0.0") {
        open(port, bindAddr);
    }

    bool open(uint16_t port, const std::string& bindAddr = "0.0.0.0") {
        close();
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) return false;
        int yes = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, bindAddr.c_str(), &addr.sin_addr) <= 0)
            addr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(fd_, (sockaddr*)&addr, sizeof(addr)) != 0) return false;
        if (::listen(fd_, 16) != 0) return false;
        return true;
    }

    TcpSocket accept() const {
        int cfd = ::accept(fd_, nullptr, nullptr);
        if (cfd < 0) throw std::runtime_error("accept failed");
        return TcpSocket(cfd);
    }
};
