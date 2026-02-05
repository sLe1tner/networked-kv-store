#include "tcp_server.hpp"

#include <stdexcept>     // std::runtime_error
#include <sys/socket.h>  // socket(), bind(), listen()
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // htons()
#include <unistd.h>      // close()

#include <string>
#include <string_view>
#include <iostream>
#include <csignal>

namespace kv {


void TcpServer::start() {
    if (running_)
        throw std::runtime_error("Server is already listening");

    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd == -1)
        throw std::runtime_error("Failed to create socket");

    listen_socket_ = Socket(fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_); // Converts port to network byte order

    int opt = 1;
    setsockopt(listen_socket_.fd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (::bind(listen_socket_.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
        throw std::runtime_error("Bind failed");

    if (::listen(listen_socket_.fd(), SOMAXCONN) == -1)
        throw std::runtime_error("Listen failed");

    std::signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE
    s_this_server = this;
    // Register the signal handler (SIGINT)
    std::signal(SIGINT, signal_handler);

    poll_fds_.push_back({listen_socket_.fd(), POLLIN, 0}); // The server listening socket
    poll_fds_.push_back({waker_.read_fd(), POLLIN, 0}); // The read-end of the self-pipe
    running_ = true;

    setup_workers();
    run_reactor();
    s_this_server = nullptr;
    stop();
}

void TcpServer::run_reactor() {
    while (running_) {
        apply_dirty_updates();
        int activity = poll(poll_fds_.data(), poll_fds_.size(), -1); // Block until a FD is ready
        if (activity < 0) {
            if (errno == EINTR) // interrupted syscall, eg: SIGWINCH or SIGCONT
                continue;
            break;
        }

        for (size_t i = 0; i < poll_fds_.size(); i++) {
            // Nothing on current fd
            if (poll_fds_[i].revents == 0)
                continue;

            // Waker poke
            if (poll_fds_[i].fd == waker_.read_fd()) {
                if (poll_fds_[i].revents & POLLIN)
                    waker_.clear();
                continue;
            }

            // New client
            if (poll_fds_[i].fd == listen_socket_.fd()) {
                if (poll_fds_[i].revents & POLLIN)
                    handle_new_connection();
                continue;
            }

            // Read from client
            if (poll_fds_[i].revents & POLLIN) {
                handle_new_command(i);
            }

            // Write to client
            if (poll_fds_[i].revents & POLLOUT) {
                handle_client_write(i);
            }

            // Handle errors (Disconnects)
            if (poll_fds_[i].revents & (POLLERR | POLLHUP)) {
                handle_client_dc(i);
            }
        }
    }
}

void TcpServer::apply_dirty_updates() {
    std::vector<int> local_dirty;
    {
        // Swap to a local vector to keep the lock time minimal
        std::lock_guard lock(dirty_mutex_);
        local_dirty.swap(dirty_fds_);
    }

    for (auto fd : local_dirty) {
        auto it = fd_idx_map_.find(fd);
        if (it != fd_idx_map_.end()) {
            poll_fds_[it->second].events |= POLLOUT;
        }
    }
}

void TcpServer::mark_as_dirty(int fd) {
    {
        std::lock_guard lock(dirty_mutex_);
        dirty_fds_.push_back(fd);
    }
    waker_.notify();
}

void TcpServer::handle_client_write(size_t& poll_fds_idx) {
    int fd = poll_fds_[poll_fds_idx].fd;
    auto& client_connection = clients_[fd];

    try {
        if (!client_connection->write_from_outbox()) {  // if "everything has been written"
            poll_fds_[poll_fds_idx].events &= ~POLLOUT; // Outbox empty, turn off POLLOUT
        }
    } catch (IOError&) {
        handle_client_dc(poll_fds_idx);
    }
}

void TcpServer::handle_client_dc(size_t& poll_fds_idx) {
    int moving_fd = poll_fds_.back().fd;
    int dead_fd = poll_fds_[poll_fds_idx].fd;

    // swap & pop to remove dead connection in O(1)
    if (poll_fds_idx < poll_fds_.size() - 1) {
        std::swap(poll_fds_[poll_fds_idx], poll_fds_.back());
        fd_idx_map_[moving_fd] = poll_fds_idx;
    }
    fd_idx_map_.erase(dead_fd);
    clients_.erase(dead_fd);
    poll_fds_.pop_back();
    poll_fds_idx--;

    std::cout << "Client [" << dead_fd << "] disconnected\n";
}

void TcpServer::handle_new_connection() {
    auto client = accept();
    if (!client)
        return;
    std::cout << "Client [" << client->fd() << "] connected on port " << port_ << "\n";
    int current_fd = client->fd();
    poll_fds_.push_back({current_fd, POLLIN, 0});
    fd_idx_map_[current_fd] = poll_fds_.size() - 1;
    clients_[current_fd] = std::make_shared<Connection>(std::move(*client));
}

void TcpServer::handle_new_command(size_t& poll_fds_idx) {
    int fd = poll_fds_[poll_fds_idx].fd;
    auto client_connection = clients_[poll_fds_[poll_fds_idx].fd];
    try {
        // Pull data from the OS into our buffer
        if (!client_connection->read_to_inbox()) {
            handle_client_dc(poll_fds_idx);
            return;
        }

        // See if we have one (or more) full commands
        while (auto line = client_connection->try_get_line()) {
            try {
                Command cmd = Protocol::parse(*line);
                if (std::holds_alternative<NoOp>(cmd))
                    continue;
                // Push to worker pool
                task_deque_.push_back(Task{
                    .connection = client_connection,
                    .cmd = cmd,
                    .on_complete = [this, fd]() { mark_as_dirty(fd); }
                });
            } catch (const ProtocolError& e) {
                client_connection->append_response(Protocol::format_error(e.what()));
                mark_as_dirty(fd);
            }
        }
    } catch (const BufferOverflowError& e) {
        client_connection->append_response(Protocol::format_error(e.what()));
        mark_as_dirty(fd);
    } catch (const IOError& e) {
        handle_client_dc(poll_fds_idx);
    }
}

void TcpServer::stop() {
    // Compare and Swap (atomic transaction) to prevent double-shutdown logic
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }
    waker_.notify();
    // stop accepting new clients
    if (listen_socket_.valid())
        listen_socket_ = Socket{}; // destroy old socket, closes FD

    workers_.clear(); // jthread auto cleanup
}

std::optional<Socket> TcpServer::accept() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create non-blocking client socket
    int client_fd = ::accept4(
        listen_socket_.fd(),
        reinterpret_cast<sockaddr*>(&client_addr),
        &client_len,
        SOCK_NONBLOCK | SOCK_CLOEXEC
    );

    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return std::nullopt;
        throw std::runtime_error("Accept failed");
    }

     return Socket{client_fd};
}

bool TcpServer::is_running() const noexcept {
    return running_;
}

void TcpServer::worker_loop(std::stop_token stop_token) {
    while(!stop_token.stop_requested()) {
        auto task = task_deque_.wait_and_pop_front(stop_token);
        if (task) {
            try {
                task->execute(store_);
            } catch (const IOError& e) {
                std::cout << e.what() << "\n";
                continue;
            }

        }
    }
}

void TcpServer::setup_workers() {
    for (size_t i = 0; i < num_workers_; i++) {
        workers_.emplace_back([this](std::stop_token stop_token) {
            worker_loop(stop_token);
        });
    }
}


} // namespace kv
