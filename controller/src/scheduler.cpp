#include "../include/scheduler.h"

#include <thread>

Scheduler::Scheduler() {
    // Start the scheduler thread that runs the loop
    thr_ = std::thread([this] { loop_(); });
}
Scheduler::~Scheduler() {
    stop(); // Ensure the scheduler is stopped and cleaned up
}

int Scheduler::every(ms interval, Job job) {
    std::lock_guard<std::mutex> lk(mx_); // Lock to ensure thread safety
    int id = next_id_++; // Generate a unique ID for the job
    items_[id] = Item{id, interval, std::move(job),
                      std::chrono::steady_clock::now() + interval, true}; // Schedule the job
    return id; // Return the job ID
}
void Scheduler::cancel(int id) {
    std::lock_guard<std::mutex> lk(mx_); // Lock to ensure thread safety
    items_.erase(id); // Remove the job from the schedule
}
void Scheduler::stop() {
    if (!running_.exchange(false)) return; // Stop the loop if it is running
    if (thr_.joinable()) thr_.join(); // Wait for the thread to finish
    std::lock_guard<std::mutex> lk(mx_); // Lock to ensure thread safety
    items_.clear(); // Clear all scheduled jobs
}
void Scheduler::loop_() {
    using clock = std::chrono::steady_clock;
    while (running_.load()) { // Continue running while the scheduler is active
        auto now = clock::now(); // Get the current time
        {
            std::lock_guard<std::mutex> lk(mx_); // Lock to ensure thread safety
            for (auto& [id, it] : items_) { // Iterate over all scheduled jobs
                if (!it.active) continue; // Skip inactive jobs
                if (now >= it.next) { // Check if the job is due to run
                    try {
                        it.job(); // Execute the job
                    } catch (...) {
                        // Catch and ignore any exceptions thrown by the job
                    }
                    it.next = now + it.interval; // Schedule the next execution
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Sleep briefly to avoid busy-waiting
    }
}
