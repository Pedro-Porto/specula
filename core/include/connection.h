#pragma once
#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

/**
 * @brief Manages a socket connection, providing thread-safe send/receive and
 * command dispatching.
 */
class Connection {
   public:
    /**
     * @brief Handler type for processing received commands.
     * @param conn Reference to the current Connection object.
     * @param payload The full payload received.
     */
    using Handler = std::function<void(Connection&, const std::string&)>;

    /**
     * @brief Constructs a Connection from an already connected socket file
     * descriptor.
     * @param fd Connected socket file descriptor. Ownership is transferred to
     * the Connection.
     */
    explicit Connection(int fd);

    /**
     * @brief Destructor. Closes the socket and stops the reader thread.
     */
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    /**
     * @brief Starts the reader thread to process incoming data.
     */
    void start();

    /**
     * @brief Stops the reader thread and closes the socket.
     */
    void stop();

    /**
     * @brief Checks if the connection is currently running.
     * @return true if running, false otherwise.
     */
    bool isRunning() const noexcept;

    /**
     * @brief Sends a command and payload in a thread-safe manner.
     * @param cmd Command string.
     * @param payload Payload string.
     * @return true if send was successful, false otherwise.
     */
    bool send(const std::string& cmd, const std::string& payload);

    /**
     * @brief Registers or updates a handler for a specific command.
     * @param cmd Command string to handle.
     * @param h Handler function.
     */
    void on(const std::string& cmd, Handler h);

    /**
     * @brief Sets the default handler for unmapped commands.
     * @param h Handler function.
     */
    void setDefaultHandler(Handler h);

    /**
     * @brief Sets the maximum allowed frame size for incoming messages.
     * @param bytes Maximum frame size in bytes. Default is 16 MiB.
     */
    void setMaxFrameSize(size_t bytes);

    /**
     * @brief Sets the read chunk size for socket reads.
     * @param bytes Chunk size in bytes. Default is 4096.
     */
    void setReadChunk(size_t bytes);

    int getCfd() { return fd_; }

    bool isAuthenticated{false};

   private:
    // states
    int fd_;
    std::atomic<bool> running_{false};
    std::thread reader_;
    std::string rxBuffer_;

    // sync
    mutable std::mutex sendMx_;
    std::mutex handlersMx_;

    // dispatch
    std::unordered_map<std::string, Handler> handlers_;
    Handler defaultHandler_{};

    // parameters
    size_t maxFrameSize_ = 16 * 1024 * 1024;  // 16 MiB
    size_t readChunk_ = 4096;
    bool asyncDispatch_ = true;

    // internals
    void readLoop();
    void dispatch(const std::string& payload);
    static bool writeAll(int fd, const void* buf, size_t n);
    static bool readSome(int fd, void* buf, size_t n, ssize_t& outRead);
};
