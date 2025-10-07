#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


/**
 * @struct CmdRecord
 * @brief Represents the record of a command's execution.
 */
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


/**
 * @class CmdRepo
 * @brief Manages the storage and state of command records.
 */
class CmdRepo {
public:
    using clock = std::chrono::steady_clock;

    /**
     * @brief Construct a new CmdRepo object
     * 
     * @param tail_limit_bytes 
     */
    explicit CmdRepo(size_t tail_limit_bytes = 64 * 1024);

    /**
     * @brief Get the next Id
     * 
     * @return int 
     */
    int nextId();


    /**
     * @brief Adds a new command record.
     *
     * @param id The unique ID of the command.
     * @param conn_id The connection ID associated with the command.
     * @param cmd The command string.
     * @param monitor Whether the command should be monitored.
     * @return int 
     */
    int add(int id, int conn_id, std::string cmd, bool monitor);

    /**
     * @brief Marks a command as started.
     *
     * @param id The unique ID of the command.
     * @return bool 
     */
    bool start(int id);

    /**
     * @brief Appends output to a command record.
     *
     * @param id The unique ID of the command.
     * @param chunk The output chunk to append.
     * @return True if the output was successfully appended, false otherwise.
     */
    bool appendOut(int id, const std::string& chunk);

    /**
     * @brief Marks a command as completed.
     *
     * @param id The unique ID of the command.
     * @param exit_code The exit code of the command.
     * @return True if the command was successfully marked as done, false otherwise.
     */
    bool done(int id, int exit_code);

    /**
     * @brief Retrieves a command record by its ID.
     *
     * @param id The unique ID of the command.
     * @return A pointer to the command record, or nullptr if not found.
     */
    std::optional<CmdRecord> get(int id) const;

    /**
     * @brief Erases a command record by its ID.
     *
     * @param id The unique ID of the command.
     * @return true se o comando foi encontrado e removido, false caso contrário.
     */
    bool erase(int id);

    /**
     * @brief Snapshot
     * 
     * @return std::vector<CmdRecord> 
     */
    std::vector<CmdRecord> snapshot() const;

    /**
     * @brief List all IDs
     * 
     * @return std::vector<int> 
     */
    std::vector<int> listIds() const;

    /**
     * @brief Remove by connection
     * 
     * @param conn_id 
     * @return size_t 
     */
    size_t removeByConn(int conn_id);

    /**
     * @brief Clear done older than
     * 
     * @param age 
     * @return size_t 
     */
    size_t clearDoneOlderThan(std::chrono::seconds age);

    /**
     * @brief Set the tail limit
     * 
     * @param bytes 
     */
    void setTailLimit(size_t bytes);

private:
    int makeId_();

    mutable std::mutex mx_;
    std::unordered_map<int, CmdRecord> by_id_;
    std::atomic<int> next_id_{1};

    size_t tail_limit_;
    void trimTail_(CmdRecord& r);
};
