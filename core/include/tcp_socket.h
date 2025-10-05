#pragma once
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

class TcpSocket {
   protected:
    int fd_ = -1;

   public:
    TcpSocket() = default;
    explicit TcpSocket(int fd) : fd_(fd) {}
    virtual ~TcpSocket() { close(); }

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }
    TcpSocket& operator=(TcpSocket&& o) noexcept {
        if (this != &o) {
            close();
            fd_ = o.fd_;
            o.fd_ = -1;
        }
        return *this;
    }

    bool isOpen() const noexcept { return fd_ >= 0; }
    int fd() const noexcept { return fd_; }

    void close() noexcept {
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool sendAll(const void* data, size_t n) const noexcept {
        const char* p = (const char*)data;
        while (n) {
            ssize_t w = ::send(fd_, p, n, 0);
            if (w < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            p += w;
            n -= (size_t)w;
        }
        return true;
    }

    ssize_t recvSome(void* buf, size_t n) const noexcept {
        ssize_t r = ::recv(fd_, buf, n, 0);
        if (r < 0 && errno == EINTR) return 0;
        return r;
    }

    int release() noexcept {
        int t = fd_;
        fd_ = -1;
        return t;
    }
};
