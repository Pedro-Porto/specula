#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


struct CmdRecord {
    enum class State : uint8_t { Pending, Running, Streaming, Done };

    int id = 0;                      // correlation id
    int conn_id = -1;                // quem executa (connection lógico)
    std::string cmd;                 // comando solicitado
    bool monitor = false;            // se true, teremos EXEC_OUT stream

    State state = State::Pending;
    int exit_code = -1;              // definido em Done

    // métricas/diagnóstico
    size_t bytes_out = 0;            // total recebido em EXEC_OUT
    size_t chunks_out = 0;           // qtde de chunks recebidos

    std::chrono::steady_clock::time_point t_created{};
    std::chrono::steady_clock::time_point t_started{};
    std::chrono::steady_clock::time_point t_last_update{};
    std::chrono::steady_clock::time_point t_finished{};

    std::string tail;
};


class CmdRepo {
public:
    using clock = std::chrono::steady_clock;

    explicit CmdRepo(size_t tail_limit_bytes = 64 * 1024);

    int nextId();


    int add(int id, int conn_id, std::string cmd, bool monitor);

    bool start(int id);

    bool appendOut(int id, const std::string& chunk);

    bool done(int id, int exit_code);

    std::optional<CmdRecord> get(int id) const;
    std::vector<CmdRecord> snapshot() const;
    std::vector<int> listIds() const;

    // Limpezas
    size_t removeByConn(int conn_id);
    bool erase(int id);
    size_t clearDoneOlderThan(std::chrono::seconds age);

    void setTailLimit(size_t bytes);

private:
    int makeId_();

    mutable std::mutex mx_;
    std::unordered_map<int, CmdRecord> by_id_;
    std::atomic<int> next_id_{1};

    size_t tail_limit_;
    void trimTail_(CmdRecord& r);
};
