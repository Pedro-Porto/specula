#include "../include/server.h"
#include <iostream>
#include <chrono>

Server::Server(CommandRegistry& registry) : registry_(registry) {}
Server::~Server(){ stop(); }

bool Server::start(uint16_t port, const std::string& bindAddr){
    if (!listener_.open(port, bindAddr)){
        std::cerr << "[server] bind/listen failed on " << port << "\n";
        return false;
    }
    running_ = true;
    accept_thr_ = std::thread([this](){
        while (running_.load()){
            int cfd = -1;
            try {
                TcpSocket sock = listener_.accept();
                cfd = sock.release();
            } catch (...) {
                if (!running_.load()) break;
                continue;
            }

            auto conn = std::make_shared<Connection>(cfd);
            registry_.attach(*conn);
            conn->start();
            conns_.add(conn);
        }
    });
    return true;
}

void Server::stop(){
    if (!running_.exchange(false)) return;
    listener_.close();
    if (accept_thr_.joinable()) accept_thr_.join();

    conns_.for_each([](auto& sp){
        if (sp) sp->stop();
    });
}

void Server::broadcast(const std::string& cmd, const std::string& payload){
    conns_.for_each([&](auto& sp){
        if (sp && sp->isRunning()){
            sp->send(cmd, payload);
        }
    });
}

bool Server::send(const std::string& cmd, const std::string& payload, int conn_id){
    conns_.for_each([&](auto& sp){
        if (sp && sp->isRunning() && sp->getCfd() == conn_id){
            return sp->send(cmd, payload);
        }
    });
}
