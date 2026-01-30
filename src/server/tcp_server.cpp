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
    if (listening_)
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

    listening_ = true;
    setup_workers();
    accept_loop();
    stop();
}

void TcpServer::accept_loop() {
    std::signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE

    // Register the signal handler (SIGINT)
    s_instance_waker = &waker_;
    std::signal(SIGINT, signal_handler);

    poll_fds_.push_back({listen_socket_.fd(), POLLIN, 0}); // The server listening socket
    poll_fds_.push_back({waker_.read_fd(), POLLIN, 0}); // The read-end of the self-pipe

    while (listening_) {
        int activity = poll(poll_fds_.data(), poll_fds_.size(), -1); // Block until a FD is ready

        if (activity < 0) {
            if (errno == EINTR) // interrupted syscall, eg: SIGWINCH or SIGCONT
                continue;
            break;
        }

        for (size_t i = 0; i < poll_fds_.size(); i++) {
            if (!poll_fds_[i].revents & POLLIN)
                continue;

            if (poll_fds_[i].fd == waker_.read_fd()) { // SIGINT
                if (listen_socket_.valid())
                    listen_socket_ = Socket{};
                waker_.clear();
                for (auto& worker : workers_)
                    worker.request_stop(); // Signals the stop_token inside the worker loop
                return;

            } else if (poll_fds_[i].fd == listen_socket_.fd()) { // New client
                auto client = accept();
                if (!client)
                    break;
                std::cout << "Client connected on port " << port_ << "\n";
                int current_fd = client->fd();
                poll_fds_.push_back({current_fd, POLLIN, 0});
                clients_[current_fd] = std::make_shared<Connection>(std::move(*client));

            } else { // Command
                auto client_connection = clients_[poll_fds_[i].fd];
                Command command;

                try { // Parse
                    std::string line = client_connection->read_line();
                    command = Protocol::parse(line);

                } catch (const ProtocolError& e) { // Typo etc
                    client_connection->write(Protocol::format_error(e.what()));
                    continue;

                } catch (const IOError& e) { // Connection/IO problem
                    std::cout << e.what() << "\n";

                    // swap & pop to remove dead connection in O(1)
                    std::swap(poll_fds_[i], poll_fds_.back());
                    poll_fds_.pop_back();
                    i--;
                    continue;
                }
                task_deque_.push_back(Task{client_connection, command});
            }
        }
    }
}

void TcpServer::stop() {
    if (!listening_)
        return;

    // stop accepting new clients
    listening_ = false;
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
        if (!listening_)
            return std::nullopt; // expected shutdown
        throw std::runtime_error("accept() failed");
    }

    return Socket{client_fd};
}

bool TcpServer::is_listening() const noexcept {
    return listening_;
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
