#include "../include/utils.h"

void broadcast(const std::string& cmd, const std::string& payload,
               ThreadSafeVector<Connection>& conns) {
    conns.for_each([&](auto& c) {
        if (c && c->isAuthenticated && c->isRunning()) {
            c->send(cmd, payload);
        }
    });
}