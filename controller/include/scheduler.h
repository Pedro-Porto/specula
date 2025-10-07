#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <unordered_map>
#include <mutex>

/**
 * @file scheduler.h
 * @brief Scheduler for managing periodic tasks.
 */

/**
 * @class Scheduler
 * @brief Manages the scheduling and execution of periodic tasks.
 */
class Scheduler {
public:
    using ms = std::chrono::milliseconds; ///< Alias for milliseconds.
    using Job = std::function<void()>; ///< Alias for a job function.

    /**
     * @brief Constructs a Scheduler and starts its loop.
     */
    Scheduler();

    /**
     * @brief Destroys the Scheduler and stops its loop.
     */
    ~Scheduler();

    /**
     * @brief Schedules a job to run periodically.
     *
     * @param interval The interval between job executions.
     * @param job The job to execute.
     * @return An ID representing the scheduled job.
     */
    int every(ms interval, Job job);

    /**
     * @brief Cancels a scheduled job.
     *
     * @param id The ID of the job to cancel.
     */
    void cancel(int id);

    /**
     * @brief Stops the Scheduler and clears all scheduled jobs.
     */
    void stop();

private:
    /**
     * @struct Item
     * @brief Represents a scheduled job.
     */
    struct Item {
        int id; ///< The unique ID of the job.
        ms interval; ///< The interval between job executions.
        Job job; ///< The job to execute.
        std::chrono::steady_clock::time_point next; ///< The next execution time.
        bool active; ///< Whether the job is active.
    };

    std::unordered_map<int, Item> items_; ///< Map of scheduled jobs by ID.
    std::mutex mx_; ///< Mutex for thread-safe access to items.
    std::atomic<bool> running_{true}; ///< Flag indicating whether the Scheduler is running.
    std::thread thr_; ///< The thread running the Scheduler loop.
    int next_id_ = 1; ///< The next job ID to assign.

    /**
     * @brief The main loop of the Scheduler.
     */
    void loop_();
};
