#pragma once

#include "kv/socket.hpp"
#include "kv/kv_store.hpp"
#include "waker.hpp"
#include "kv/task_deque.hpp"
#include "kv/protocol.hpp"
#include "kv/command_dispatcher.hpp"
#include "connection.hpp"
#include <cstdint>
#include <thread>
#include <deque>
#include <mutex>
#include <vector>
#include <functional>
#include <condition_variable>
#include <optional>
#include <atomic>
#include <poll.h>
#include <map>

#include <iostream>
namespace kv {

struct Task {
    std::weak_ptr<Connection> connection;
    Command cmd;
    std::function<void()> on_complete; // Reactor poke callback
    void execute(KvStore& store) {
        if (auto client = connection.lock()) {
            std::string response = CommandDispatcher::execute(cmd, store);
            if (response.empty())
                return;
            client->append_response(response);
            if (on_complete)
                on_complete();
        } else {
            // The Reactor already deleted this connection
            std::cout << "[Worker] Skipping task: Client already disconnected." << std::endl;
        }
    };
};


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

    // Stop listening (closes socket), stop workers
    void stop();

    // Accept a new client connection.
    std::optional<Socket> accept();

    // Returns true if the server is running.
    bool is_running() const noexcept;

private:
    KvStore store_;
    Socket listen_socket_;
    std::atomic<bool> running_{false};
    uint16_t port_{0};

    // Reactor event loop
    void run_reactor();
    void handle_new_connection();
    void handle_new_command(int& poll_fds_idx);
    void handle_client_write(int& poll_fds_idx);
    void handle_client_dc(int& poll_fds_idx);

    // Thread pool
    size_t num_workers_{5};
    std::deque<std::function<void()>> tasks_{};
    TaskDeque<Task> task_deque_;
    std::vector<std::jthread> workers_;
    std::vector<pollfd> poll_fds_;
    std::map<int, std::shared_ptr<Connection>> clients_; // fd -> connection map
    void setup_workers();
    void worker_loop(std::stop_token stop_token);

    // Waker
    Waker waker_;
    inline static TcpServer* s_this_server = nullptr; // used for waker callback in task
    static void signal_handler(int) {
        if (s_this_server)
            s_this_server->stop();
    }

    // dirty list
    std::mutex dirty_mutex_;
    std::vector<int> dirty_fds_;
    std::unordered_map<int, size_t> fd_idx_map_;

    void mark_as_dirty(int fd);
    void apply_dirty_updates();

};

} // namespace kv
