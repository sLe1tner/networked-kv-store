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

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
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
        set_poll_fd_flags();
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

void TcpServer::set_poll_fd_flags() {
    for (auto& poll_fd : poll_fds_) {
        if (poll_fd.fd == waker_.read_fd() || poll_fd.fd == listen_socket_.fd())
            continue;

        auto it = clients_.find(poll_fd.fd);
        if (it != clients_.end() && it->second->has_pending_data()) {
            poll_fd.events = POLLIN | POLLOUT;
        } else {
            poll_fd.events = POLLIN;
        }
    }
}

void TcpServer::handle_client_write(size_t& poll_fds_idx) {
    auto client_connection = clients_[poll_fds_[poll_fds_idx].fd];
    client_connection->write_from_outbox();
}

void TcpServer::handle_client_dc(size_t& poll_fds_idx) {
    int fd = poll_fds_[poll_fds_idx].fd;
    clients_.erase(fd);

    // swap & pop to remove dead connection in O(1)
    if (poll_fds_idx < poll_fds_.size() - 1)
        std::swap(poll_fds_[poll_fds_idx], poll_fds_.back());
    poll_fds_.pop_back();
    poll_fds_idx--;

    std::cout << "Client [" << fd << "] disconnected\n";
}


void TcpServer::handle_new_connection() {
    auto client = accept();
    if (!client)
        return;
    std::cout << "Client [" << client->fd() << "] connected on port " << port_ << "\n";
    int current_fd = client->fd();
    poll_fds_.push_back({current_fd, POLLIN, 0});
    clients_[current_fd] = std::make_shared<Connection>(std::move(*client));
}

void TcpServer::handle_new_command(size_t& poll_fds_idx) {
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

                // Push to worker pool
                task_deque_.push_back(Task{
                    .connection = client_connection,
                    .cmd = cmd,
                    .on_complete = [this]() { this->waker_.notify(); }
                });
            } catch (const ProtocolError& e) {
                client_connection->append_response(Protocol::format_error(e.what()));
            }
        }
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
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = ::accept(
        listen_socket_.fd(),
        reinterpret_cast<sockaddr*>(&client_addr),
        &client_len
    );

    if (client_fd == -1) {
        if (!running_)
            return std::nullopt; // expected shutdown
        throw std::runtime_error("accept() failed");
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
