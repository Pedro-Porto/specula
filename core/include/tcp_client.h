#pragma once
#include "tcp_socket.h"
#include <netdb.h>
#include <string>
#include <future>

class TcpClient : public TcpSocket {
public:
    TcpClient() = default;

    bool connectTo(const std::string& host, uint16_t port, int timeoutMs = 5000) {
        close();
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        std::string portStr = std::to_string(port);
        if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) return false;

        bool ok = false;
        for (auto p = res; p; p = p->ai_next) {
            fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd_ < 0) continue;

            struct timeval tv{};
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            if (::connect(fd_, p->ai_addr, p->ai_addrlen) == 0) { ok = true; break; }
            ::close(fd_); fd_ = -1;
        }
        freeaddrinfo(res);
        return ok;
    }
};
