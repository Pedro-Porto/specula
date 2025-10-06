#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include <optional>

struct Stats {
    int         conn_id;
    float       cpu_percent;
    uint64_t    mem_used_bytes;
    uint64_t    mem_total_bytes;
    uint64_t    disk_used_bytes;
    uint64_t    disk_total_bytes;
};

class StatsRepo {
public:
    void upsert(const Stats& s);
    void removeByConnId(int id);
    std::vector<Stats> snapshot() const;
    std::optional<Stats> get(int id) const;

private:
    mutable std::mutex mx_;
    std::vector<Stats> data_;
};
