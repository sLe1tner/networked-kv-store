#include "tcp_server.hpp"
#include "connection.hpp"
#include "kv/protocol.hpp"
#include "kv/command_dispatcher.hpp"

#include <stdexcept>     // std::runtime_error
#include <sys/socket.h>  // socket(), bind(), listen()
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // htons()
#include <unistd.h>      // close()

#include <string>
#include <string_view>
#include <iostream>

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

    running_ = true;
    setup_workers();
    listening_ = true;
    accept_loop();
}

void TcpServer::accept_loop() {
    while (listening_) {
        auto client = accept();
        if (!client)
            break; // server shutting down
        std::cout << "Client connected on port " << port_ << "\n";

        // std::function requires its callable to be copy-constructible, which Socket is not.
        // So I'm using shared_ptr here as a "copyable ownership token".
        // In C++23 std::function can be std::move_only_function instead.
        auto client_connection = std::make_shared<Connection>(std::move(*client));
        submit_task([this, client_connection]() mutable {
            handle_client(*client_connection);
        });
    }
}

void TcpServer::stop() {
    if (!listening_)
        return;

    // stop accepting new clients
    listening_ = false;
    if (listen_socket_.valid())
        listen_socket_ = Socket{}; // destroy old socket, closes FD

    // Push a poison pill for each worker
    {
        std::lock_guard lock(tasks_mutex_);
        for (size_t i = 0; i < num_workers_; ++i) {
            tasks_.push_back(POISON_PILL);
        }
    }
    cv_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable())
            t.join();
    }
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

void TcpServer::handle_client(Connection &connection) {
    while (true) {
        try {
            std::string line = connection.read_line();
            Command command = Protocol::parse(line);
            std::string response = CommandDispatcher::execute(command, store_);
            connection.write(response);
        } catch (const ProtocolError& e) {
            connection.write(Protocol::format_error(e.what()));
        } catch (const IOError&) {
            break; // Client disconnected or I/O failure
        }
    }
}

void TcpServer::worker_loop() {
    while(true) {
        std::function<void()> task;
        {
            std::unique_lock lock(tasks_mutex_);
            cv_.wait(lock, [this] { return !tasks_.empty(); });

            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        // Poison pill check
        if (!task)
            break; // Exit the loop and kill the thread

        task();
    }
}

void TcpServer::setup_workers() {
    for (size_t i = 0; i < num_workers_; i++) {
        workers_.emplace_back([this] {
            worker_loop();
        });
    }
}

void TcpServer::submit_task(std::function<void()> task) {
    {
        std::unique_lock lock(tasks_mutex_);
        tasks_.push_back(std::move(task));
    }
    cv_.notify_one();
}

} // namespace kv
