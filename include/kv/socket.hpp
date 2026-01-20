# pragma once


namespace kv {

/*
 * RAII wrapper around a TCP socket file descriptor.
 * Public interface for a socket abstraction.
 * TODO: Will later manage: file descriptor lifetime, move-only semantics, send / recv
 */
class Socket {
public:
    Socket() = default;
    ~Socket() = default;

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&&) noexcept = default;
    Socket& operator=(Socket&&) noexcept = default;
};

} // namespace kv
