#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <unordered_map>
#include <mutex>

class Scheduler {
public:
    using Job = std::function<void()>;
    using ms  = std::chrono::milliseconds;

    Scheduler();
    ~Scheduler();

    int  every(ms interval, Job job);
    void cancel(int id);
    void stop();

private:
    struct Item { int id; ms interval; Job job; std::chrono::steady_clock::time_point next; bool active; };
    std::atomic<bool> running_{true};
    std::thread thr_;
    std::mutex mx_;
    int next_id_ = 1;
    std::unordered_map<int, Item> items_;
    void loop_();
};
