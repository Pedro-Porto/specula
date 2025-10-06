#include "../include/stats_repo.h"

#include <algorithm>

void StatsRepo::upsert(const Stats& s) {
    std::lock_guard<std::mutex> lk(mx_);
    auto it = std::find_if(data_.begin(), data_.end(), [&](const Stats& x) {
        return x.conn_id == s.conn_id;
    });
    if (it != data_.end())
        *it = s;
    else
        data_.push_back(s);
}
void StatsRepo::removeByConnId(int id) {
    std::lock_guard<std::mutex> lk(mx_);
    data_.erase(std::remove_if(data_.begin(), data_.end(),
                               [&](const Stats& x) { return x.conn_id == id; }),
                data_.end());
}
std::vector<Stats> StatsRepo::snapshot() const {
    std::lock_guard<std::mutex> lk(mx_);
    return data_;
}
std::optional<Stats> StatsRepo::get(int id) const {
    std::lock_guard<std::mutex> lk(mx_);
    auto it = std::find_if(data_.begin(), data_.end(),
                           [&](const Stats& x) { return x.conn_id == id; });
    if (it == data_.end()) return std::nullopt;
    return *it;
}
