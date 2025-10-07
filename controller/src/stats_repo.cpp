#include "../include/stats_repo.h"

#include <algorithm>

// Upsert function either updates an existing Stats object or adds a new one
// if no object with the same conn_id exists.
void StatsRepo::upsert(const Stats& s) {
    std::lock_guard<std::mutex> lk(mx_); // Locking the mutex to ensure thread safety
    // Find if the Stats object with the same conn_id already exists
    auto it = std::find_if(data_.begin(), data_.end(), [&](const Stats& x) {
        return x.conn_id == s.conn_id;
    });
    // If found, update the existing object, otherwise add the new object to the data_
    if (it != data_.end())
        *it = s;
    else
        data_.push_back(s);
}

// removeByConnId function removes the Stats object associated with the given conn_id.
void StatsRepo::removeByConnId(int id) {
    std::lock_guard<std::mutex> lk(mx_); // Locking the mutex to ensure thread safety
    // Remove the Stats object where conn_id matches the given id
    data_.erase(std::remove_if(data_.begin(), data_.end(),
                               [&](const Stats& x) { return x.conn_id == id; }),
                data_.end());
}

// snapshot function returns a copy of the current data_ vector.
std::vector<Stats> StatsRepo::snapshot() const {
    std::lock_guard<std::mutex> lk(mx_); // Locking the mutex to ensure thread safety
    return data_; // Return a copy of the data_
}

// get function retrieves the Stats object associated with the given conn_id.
std::optional<Stats> StatsRepo::get(int id) const {
    std::lock_guard<std::mutex> lk(mx_); // Locking the mutex to ensure thread safety
    // Find the Stats object with the matching conn_id
    auto it = std::find_if(data_.begin(), data_.end(),
                           [&](const Stats& x) { return x.conn_id == id; });
    // If not found, return std::nullopt, otherwise return the found object
    if (it == data_.end()) return std::nullopt;
    return *it;
}
