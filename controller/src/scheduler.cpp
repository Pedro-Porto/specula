#include "../include/scheduler.h"

#include <thread>

Scheduler::Scheduler() {
    thr_ = std::thread([this] { loop_(); });
}
Scheduler::~Scheduler() { stop(); }

int Scheduler::every(ms interval, Job job) {
    std::lock_guard<std::mutex> lk(mx_);
    int id = next_id_++;
    items_[id] = Item{id, interval, std::move(job),
                      std::chrono::steady_clock::now() + interval, true};
    return id;
}
void Scheduler::cancel(int id) {
    std::lock_guard<std::mutex> lk(mx_);
    items_.erase(id);
}
void Scheduler::stop() {
    if (!running_.exchange(false)) return;
    if (thr_.joinable()) thr_.join();
    std::lock_guard<std::mutex> lk(mx_);
    items_.clear();
}
void Scheduler::loop_() {
    using clock = std::chrono::steady_clock;
    while (running_.load()) {
        auto now = clock::now();
        {
            std::lock_guard<std::mutex> lk(mx_);
            for (auto& [id, it] : items_) {
                if (!it.active) continue;
                if (now >= it.next) {
                    try {
                        it.job();
                    } catch (...) {
                    }
                    it.next = now + it.interval;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
