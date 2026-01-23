#pragma once

#include "kv/socket.hpp"
#include <cstdint>

namespace kv {

/*
 * TCP server skeleton.
 */
class TcpServer {
public:
    TcpServer() = default;
    explicit TcpServer(uint16_t port);

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

    // Returns true if the server is listening.
    bool is_listening() const noexcept;

private:
    Socket listen_socket_;
    bool listening_ = false;

};

} // namespace kv
