#include "../include/cmd_repo.h"

#include <algorithm>

CmdRepo::CmdRepo(size_t tail_limit_bytes)
: tail_limit_(tail_limit_bytes) {}

int CmdRepo::makeId_() {
    return next_id_.fetch_add(1, std::memory_order_relaxed);
}

int CmdRepo::nextId() { return makeId_(); }

int CmdRepo::add(int id, int conn_id, std::string cmd, bool monitor) {
    const auto now = clock::now();
    if (id <= 0) id = makeId_();

    CmdRecord rec;
    rec.id = id;
    rec.conn_id = conn_id;
    rec.cmd = std::move(cmd);
    rec.monitor = monitor;
    rec.state = CmdRecord::State::Pending;
    rec.t_created = now;
    rec.t_last_update = now;

    std::lock_guard<std::mutex> lk(mx_);
    by_id_.erase(id);
    by_id_.emplace(id, std::move(rec));
    return id;
}

bool CmdRepo::start(int id) {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false;

    auto& r = it->second;
    r.state = CmdRecord::State::Running;
    r.t_started = now;
    r.t_last_update = now;
    return true;
}

bool CmdRepo::appendOut(int id, const std::string& chunk) {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false;

    auto& r = it->second;
    r.bytes_out += chunk.size();
    r.chunks_out += 1;
    if (r.monitor) {
        r.state = CmdRecord::State::Streaming;
        r.tail.append(chunk);
        trimTail_(r);
    }
    r.t_last_update = now;
    return true;
}

bool CmdRepo::done(int id, int exit_code) {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return false;

    auto& r = it->second;
    r.exit_code = exit_code;
    r.state = CmdRecord::State::Done;
    r.t_finished = now;
    r.t_last_update = now;
    return true;
}

std::optional<CmdRecord> CmdRepo::get(int id) const {
    std::lock_guard<std::mutex> lk(mx_);
    auto it = by_id_.find(id);
    if (it == by_id_.end()) return std::nullopt;
    return it->second;
}

std::vector<CmdRecord> CmdRepo::snapshot() const {
    std::vector<CmdRecord> out;
    std::lock_guard<std::mutex> lk(mx_);
    out.reserve(by_id_.size());
    for (const auto& kv : by_id_) out.push_back(kv.second);
    return out;
}

std::vector<int> CmdRepo::listIds() const {
    std::vector<int> ids;
    std::lock_guard<std::mutex> lk(mx_);
    ids.reserve(by_id_.size());
    for (const auto& kv : by_id_) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

size_t CmdRepo::removeByConn(int conn_id) {
    std::lock_guard<std::mutex> lk(mx_);
    size_t before = by_id_.size();
    for (auto it = by_id_.begin(); it != by_id_.end(); ) {
        if (it->second.conn_id == conn_id) it = by_id_.erase(it);
        else ++it;
    }
    return before - by_id_.size();
}

bool CmdRepo::erase(int id) {
    std::lock_guard<std::mutex> lk(mx_);
    return by_id_.erase(id) > 0;
}

size_t CmdRepo::clearDoneOlderThan(std::chrono::seconds age) {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mx_);
    size_t removed = 0;
    for (auto it = by_id_.begin(); it != by_id_.end(); ) {
        const auto& r = it->second;
        if (r.state == CmdRecord::State::Done &&
            r.t_finished != CmdRecord{}.t_finished &&
            (now - r.t_finished) > age) {
            it = by_id_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

void CmdRepo::setTailLimit(size_t bytes) {
    std::lock_guard<std::mutex> lk(mx_);
    tail_limit_ = bytes;
    for (auto& kv : by_id_) trimTail_(kv.second);
}

void CmdRepo::trimTail_(CmdRecord& r) {
    if (tail_limit_ == 0) { r.tail.clear(); return; }
    if (r.tail.size() <= tail_limit_) return;
    r.tail.erase(0, r.tail.size() - tail_limit_);
}
