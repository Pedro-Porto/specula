#pragma once
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

// TcpSocket class represents a TCP socket and provides methods for
// sending and receiving data over the network.
class TcpSocket {
   protected:
    int fd_ = -1;  // File descriptor for the socket, initialized to -1 (not open)

   public:
    TcpSocket() = default;  // Default constructor
    explicit TcpSocket(int fd) : fd_(fd) {}  // Constructor to initialize with an existing file descriptor
    virtual ~TcpSocket() { close(); }  // Destructor ensures the socket is closed

    TcpSocket(const TcpSocket&) = delete;  // Disable copy constructor
    TcpSocket& operator=(const TcpSocket&) = delete;  // Disable copy assignment operator
    TcpSocket(TcpSocket&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }  // Move constructor transfers ownership
    TcpSocket& operator=(TcpSocket&& o) noexcept {
        if (this != &o) {
            close();  // Close the current socket if open
            fd_ = o.fd_;  // Transfer ownership of the file descriptor
            o.fd_ = -1;  // Invalidate the source object
        }
        return *this;
    }

    // isOpen() checks if the socket is open.
    bool isOpen() const noexcept { return fd_ >= 0; }
    // fd() returns the file descriptor of the socket.
    int fd() const noexcept { return fd_; }

    // close() shuts down and closes the socket.
    void close() noexcept {
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);  // Disable further send/receive operations
            ::close(fd_);  // Close the file descriptor
            fd_ = -1;  // Mark the socket as closed
        }
    }

    // sendAll() sends all data in the buffer over the socket.
    bool sendAll(const void* data, size_t n) const noexcept {
        const char* p = (const char*)data;  // Pointer to the data buffer
        while (n) {
            ssize_t w = ::send(fd_, p, n, 0);  // Send data
            if (w < 0) {
                if (errno == EINTR) continue;  // Retry if interrupted
                return false;  // Return false on error
            }
            p += w;  // Move the pointer forward
            n -= (size_t)w;  // Decrease the remaining bytes to send
        }
        return true;  // All data sent successfully
    }

    // recvSome() receives some data from the socket into the buffer.
    ssize_t recvSome(void* buf, size_t n) const noexcept {
        ssize_t r = ::recv(fd_, buf, n, 0);  // Receive data
        if (r < 0 && errno == EINTR) return 0;  // Return 0 if interrupted
        return r;  // Return the number of bytes received
    }

    // release() releases the ownership of the socket, returning the file descriptor.
    int release() noexcept {
        int t = fd_;  // Store the current file descriptor
        fd_ = -1;  // Invalidate the socket
        return t;  // Return the file descriptor
    }
};
