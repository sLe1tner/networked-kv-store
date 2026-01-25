#pragma once

#include "kv/socket.hpp"
#include <cstdint>

namespace kv {

class Connection;

/*
 * TCP server skeleton.
 */
class TcpServer {
public:
    TcpServer() = default;
    explicit TcpServer(uint16_t port) { start(port); };

    ~TcpServer() = default;

    TcpServer(const TcpServer&) = delete;
    TcpServer operator=(const TcpServer&) = delete;

    TcpServer(TcpServer&&) = delete;
    TcpServer operator=(TcpServer&&) = delete;

    // Bind to the given port and start listening.
    // Throws std::runtime_error on failure.
    void start(uint16_t port);

    // Stop listening (closes socket).
    void stop();

    // Accept a new client connection.
    // Blocks until a client connects.
    Socket accept();

    // Returns true if the server is listening.
    bool is_listening() const noexcept;

private:
    Socket listen_socket_;
    bool listening_ = false;
    uint16_t port_ = 0;

    void handle_client(Connection &conn);
    void accept_loop();

};

} // namespace kv
