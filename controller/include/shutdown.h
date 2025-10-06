#pragma once
#include <atomic>

struct Shutdown {
    std::atomic<bool> running{true};
    void request(){ running = false; }
};
