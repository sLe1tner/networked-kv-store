#pragma once

#include "kv/socket.hpp"
#include "kv/kv_store.hpp"
#include "waker.hpp"
#include <cstdint>
#include <thread>
#include <deque>
#include <mutex>
#include <vector>
#include <functional>
#include <condition_variable>
#include <optional>
#include <atomic>

namespace kv {

class Connection;

/*
 * TCP server skeleton.
 */
class TcpServer {
public:
    explicit TcpServer(uint16_t port, size_t num_workers = 5)
        : port_(port), num_workers_(num_workers) {};

    ~TcpServer() = default;

    TcpServer(const TcpServer&) = delete;
    TcpServer operator=(const TcpServer&) = delete;

    TcpServer(TcpServer&&) = delete;
    TcpServer operator=(TcpServer&&) = delete;

    // Bind to the given port and start listening.
    // Throws std::runtime_error on failure.
    void start();

    // Stop listening (closes socket).
    void stop();

    // Accept a new client connection.
    // Blocks until a client connects.
    std::optional<Socket> accept();

    // Returns true if the server is listening.
    bool is_listening() const noexcept;

private:
    Socket listen_socket_;
    std::atomic<bool> listening_{false};
    uint16_t port_{0};
    KvStore store_;

    void handle_client(Connection &conn);
    void accept_loop();

    // thread pool
    size_t num_workers_{5};
    std::deque<std::function<void()>> tasks_{};
    std::vector<std::thread> workers_;
    std::mutex tasks_mutex_;
    std::condition_variable cv_;
    inline static const std::function<void()> POISON_PILL{nullptr};
    void worker_loop();
    void setup_workers();
    void submit_task(std::function<void()> task);

    // waker
    Waker waker_;
    inline static Waker* s_instance_waker = nullptr;
    static void signal_handler(int) {
        if (s_instance_waker)
            s_instance_waker->notify();
    }

};

} // namespace kv
