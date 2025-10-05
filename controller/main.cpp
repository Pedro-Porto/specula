#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../core/include/connection.h"
#include "../core/include/tcp_listener.h"
#include "../core/include/tcp_socket.h"

static std::atomic<bool> g_running{true};
static void on_sigint(int) { g_running = false; }

static std::string after_first_newline(const std::string& s) {
    auto pos = s.find('\n');
    if (pos == std::string::npos) return "";
    return s.substr(pos + 1);
}

int main() {
    signal(SIGINT, on_sigint);
    const uint16_t PORT = 60119;
    const std::string TOKEN = "supersecret";

    TcpListener listener;
    if (!listener.open(PORT, "0.0.0.0")) {
        std::cerr << "[server] failed to bind/listen on " << PORT << "\n";
        return 1;
    }
    std::cout << "[server] listening on " << PORT << "\n";

    std::vector<std::shared_ptr<Connection>> conns;
    std::mutex conns_mx;

    std::thread acceptor([&]() {
        while (g_running.load()) {
            int cfd = -1;
            try {
                TcpSocket sock = listener.accept();
                cfd = sock.release();
            } catch (...) {
                if (!g_running.load()) break;
                continue;
            }
            std::cout << "[server] client connected, fd=" << cfd << "\n";

            auto conn = std::make_shared<Connection>(cfd);

            conn->on("AUTH", [&, TOKEN](Connection& c, const std::string& p) {
                std::string token = after_first_newline(p);
                if (token == TOKEN)
                    c.send("OK", "agent\n");
                else
                    c.send("ERR", "unauthorized\n");
            });
            conn->on("PING", [](Connection& c, const std::string&) {
                c.send("PONG", "");
            });
            conn->on("STATUS", [](Connection& c, const std::string&) {
                c.send("STATUS",
                       "cpu=12% mem=1.2GiB/4.0GiB disk=9.8GiB/30GiB\n");
            });
            conn->on("BYE", [](Connection& c, const std::string&) {
                c.send("OK", "bye\n");
            });
            conn->setDefaultHandler([](Connection& c, const std::string&) {
                c.send("ERR", "unknown_cmd\n");
            });

            conn->start();

            {
                std::lock_guard<std::mutex> lk(conns_mx);
                conns.push_back(conn);
            }
        }
    });

    while (g_running.load()) {
        {
            std::lock_guard<std::mutex> lk(conns_mx);
            conns.erase(
                std::remove_if(conns.begin(), conns.end(),
                               [](const std::shared_ptr<Connection>& c) {
                                   return !c || !c->isRunning();
                               }),
                conns.end());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    listener.close();
    if (acceptor.joinable()) acceptor.join();

    {
        std::lock_guard<std::mutex> lk(conns_mx);
        for (auto& c : conns)
            if (c) c->stop();
        conns.clear();
    }

    std::cout << "[server] shutdown\n";
    return 0;
}
