#include "../include/cmd_repo.h"

#include <algorithm>

// Constructor for CmdRepo, initializes tail_limit_ and other members
CmdRepo::CmdRepo(size_t tail_limit_bytes)
: tail_limit_(tail_limit_bytes) {}

// Generates a new ID for a command record
int CmdRepo::makeId_() {
    return next_id_.fetch_add(1, std::memory_order_relaxed);
}

// Returns the next available ID
int CmdRepo::nextId() { return makeId_(); }

// Adds a new command record or updates an existing one
int CmdRepo::add(int id, int conn_id, std::string cmd, bool monitor) {
    const auto now = clock::now();
    if (id <= 0) id = makeId_(); // Generate ID if not provided

    CmdRecord rec;
    rec.id = id;
    rec.conn_id = conn_id;
    rec.cmd = std::move(cmd);
    rec.monitor = monitor;
    rec.state = CmdRecord::State::Pending;
    rec.t_created = now;
    rec.t_last_update = now;

    std::lock_guard<std::mutex> lk(mx_);
    by_id_.erase(id); // Remove existing record with the same ID, if any
    by_id_.emplace(id, std::move(rec)); // Insert the new or updated record
    return id;
}

// Marks a command as started
bool CmdRepo::start(int id) {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false; // ID not found

    auto& r = it->second;
    r.state = CmdRecord::State::Running;
    r.t_started = now;
    r.t_last_update = now;
    return true;
}

// Appends output data to a command record
bool CmdRepo::appendOut(int id, const std::string& chunk) {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false; // ID not found

    auto& r = it->second;
    r.bytes_out += chunk.size();
    r.chunks_out += 1;
    if (r.monitor) {
        r.state = CmdRecord::State::Streaming;
        r.tail.append(chunk); // Append chunk to tail
        trimTail_(r); // Trim tail if it exceeds the limit
    }
    r.t_last_update = now;
    return true;
}

// Marks a command as done and records the exit code
bool CmdRepo::done(int id, int exit_code) {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false; // ID not found

    auto& r = it->second;
    r.exit_code = exit_code;
    r.state = CmdRecord::State::Done;
    r.t_finished = now;
    r.t_last_update = now;
    return true;
}

// Retrieves a command record by ID
std::optional<CmdRecord> CmdRepo::get(int id) const {
    std::lock_guard<std::mutex> lk(mx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return std::nullopt; // ID not found
    return it->second;
}

// Returns a snapshot of all command records
std::vector<CmdRecord> CmdRepo::snapshot() const {
    std::vector<CmdRecord> out;
    std::lock_guard<std::mutex> lk(mx_);
    out.reserve(by_id_.size());
    for (const auto& kv : by_id_) out.push_back(kv.second);
    return out;
}

// Returns a sorted list of all command IDs
std::vector<int> CmdRepo::listIds() const {
    std::vector<int> ids;
    std::lock_guard<std::mutex> lk(mx_);
    ids.reserve(by_id_.size());
    for (const auto& kv : by_id_) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// Removes command records by connection ID
size_t CmdRepo::removeByConn(int conn_id) {
    std::lock_guard<std::mutex> lk(mx_);
    size_t before = by_id_.size();
    for (auto it = by_id_.begin(); it != by_id_.end(); ) {
        if (it->second.conn_id == conn_id) it = by_id_.erase(it); // Erase record and get next iterator
        else ++it;
    }
    return before - by_id_.size(); // Return the number of removed records
}

// Erases a command record by ID
bool CmdRepo::erase(int id) {
    std::lock_guard<std::mutex> lk(mx_);
    return by_id_.erase(id) > 0; // Return true if a record was erased
}

// Clears done command records older than the specified age
size_t CmdRepo::clearDoneOlderThan(std::chrono::seconds age) {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mx_);
    size_t removed = 0;
    for (auto it = by_id_.begin(); it != by_id_.end(); ) {
        const auto& r = it->second;
        // Check if the record is done, has a valid finish time, and is older than the specified age
        if (r.state == CmdRecord::State::Done &&
            r.t_finished != CmdRecord{}.t_finished &&
            (now - r.t_finished) > age) {
            it = by_id_.erase(it); // Erase record and get next iterator
            ++removed;
        } else {
            ++it;
        }
    }
    return removed; // Return the number of removed records
}

// Sets the limit for the tail size and trims existing tails if necessary
void CmdRepo::setTailLimit(size_t bytes) {
    std::lock_guard<std::mutex> lk(mx_);
    tail_limit_ = bytes;
    for (auto& kv : by_id_) trimTail_(kv.second); // Trim tail of each record
}

// Trims the tail of a command record to fit the size limit
void CmdRepo::trimTail_(CmdRecord& r) {
    if (tail_limit_ == 0) { r.tail.clear(); return; } // Clear tail if limit is 0
    if (r.tail.size() <= tail_limit_) return; // No trimming needed
    r.tail.erase(0, r.tail.size() - tail_limit_); // Trim the tail
}
