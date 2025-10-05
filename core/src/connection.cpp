#include "../include/connection.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <future>
#include <iostream>
#include <sstream>
#include <vector>

Connection::Connection(int fd) : fd_(fd) {}
Connection::~Connection() { stop(); }

void Connection::start() {
    if (running_.exchange(true)) return;  // already running
    reader_ = std::thread([this] { readLoop(); });
}

void Connection::stop() {
    running_.store(false, std::memory_order_relaxed);
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
    }
    if (reader_.joinable()) {
        if (std::this_thread::get_id() == reader_.get_id()) {
            reader_.detach();
        } else {
            reader_.join();
        }
    }

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Connection::isRunning() const noexcept { return running_.load(); }

bool Connection::writeAll(int fd, const void* buf, size_t n) {
    const char* p = static_cast<const char*>(buf);
    while (n) {
        ssize_t w = ::send(fd, p, n, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += w;
        n -= static_cast<size_t>(w);
    }
    return true;
}

bool Connection::readSome(int fd, void* buf, size_t n, ssize_t& outRead) {
    outRead = ::recv(fd, buf, n, 0);
    if (outRead < 0 && errno == EINTR) {
        outRead = 0;
        return true;
    }
    return !(outRead < 0);
}

bool Connection::send(const std::string& cmd, const std::string& payload) {
    std::string fullPayload = cmd + "\n" + payload;
    std::lock_guard<std::mutex> lk(sendMx_);
    std::string pSize = std::to_string(fullPayload.size()) + "\n";
    return writeAll(fd_, pSize.data(), pSize.size()) &&
           writeAll(fd_, fullPayload.data(), fullPayload.size());
}

void Connection::on(const std::string& cmd, Handler h) {
    std::lock_guard<std::mutex> lk(handlersMx_);
    handlers_[cmd] = std::move(h);
}

void Connection::setDefaultHandler(Handler h) {
    std::lock_guard<std::mutex> lk(handlersMx_);
    defaultHandler_ = std::move(h);
}

void Connection::setMaxFrameSize(size_t bytes) { maxFrameSize_ = bytes; }
void Connection::setReadChunk(size_t bytes) {
    readChunk_ = bytes ? bytes : 4096;
}

void Connection::dispatch(const std::string& payload) {
    // First token is the command
    std::istringstream iss(payload);
    std::string cmd;
    if (!(iss >> cmd)) return;

    Handler h;
    {
        std::lock_guard<std::mutex> lk(handlersMx_);
        auto it = handlers_.find(cmd);
        h = (it != handlers_.end()) ? it->second : defaultHandler_;
    }
    if (!h) return;

    auto copy = payload;
    [[maybe_unused]] auto fut =
        std::async(std::launch::async, [this, copy, h] { h(*this, copy); });
}

void Connection::readLoop() {
    std::vector<char> tmp(readChunk_);

    while (running_) {
        ssize_t got = 0;
        if (!readSome(fd_, tmp.data(), tmp.size(), got)) {
            // fatal recv error
            break;
        }
        if (got == 0) {
            // 0 can be EINTR treated as 0 (loop continues) OR peer closed
            // if peer closed (recv==0), exit
            if (errno != EINTR) {  // if not EINTR masked, peer closed
                break;
            }
            continue;
        }
        rxBuffer_.append(tmp.data(), static_cast<size_t>(got));

        // parse multiple accumulated frames
        while (true) {
            // 1) find '\n' in header
            auto posNL = rxBuffer_.find('\n');
            if (posNL == std::string::npos) break;  // incomplete header

            // 2) convert LEN
            size_t len = 0;
            try {
                std::string hdr = rxBuffer_.substr(0, posNL);
                if (hdr.empty() || hdr.size() > 32) {
                    running_ = false;
                    break;
                }
                size_t idx = 0;
                len = std::stoull(hdr, &idx, 10);
                if (idx != hdr.size()) {
                    running_ = false;
                    break;
                }  // garbage in header
            } catch (...) {
                running_ = false;
                break;  // invalid header
            }

            if (len > maxFrameSize_) {
                running_ = false;
                break;
            }  // protection

            // 3) check if we already have the full payload
            const size_t need = posNL + 1 + len;
            if (rxBuffer_.size() < need) break;  // not yet

            // 4) extract payload
            std::string payload = rxBuffer_.substr(posNL + 1, len);

            // 5) consume from buffer
            rxBuffer_.erase(0, need);

            // 6) dispatch
            dispatch(payload);
        }
    }

    running_ = false;
}
