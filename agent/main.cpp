#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "../core/include/tcp_client.h"
#include "../core/include/connection.h"

int main() {
    const char* HOST = "127.0.0.1";
    const uint16_t PORT = 60119;

    TcpClient cli;
    if (!cli.connectTo(HOST, PORT)) {
        std::cerr << "[client] connect failed\n";
        return 1;
    }
    std::cout << "[client] connected, fd=" << cli.fd() << "\n";

    Connection conn(cli.release());
    (void) new (&cli) TcpClient();

    conn.setDefaultHandler([](Connection& , const std::string& payload) {
        std::cout << "[server] " << payload;
    });

    conn.start();

    conn.send("AUTH", "supersecret");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    conn.send("PING", "");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    conn.send("STATUS", "");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    conn.send("BYE", "");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    conn.stop();
    std::cout << "[client] done\n";
    return 0;
}
