# pragma once


namespace kv {

/*
 * RAII wrapper for a POSIX TCP socket
 *
 * Owns the descriptor and closes it on destruction
 * Move-only
 */
class Socket {
public:
    // Constructs an invalid socket
    Socket() noexcept;

    // Takes ownership of an existing file descriptor
    explicit Socket(int fd) noexcept;

    // Closes the socket if valid
    ~Socket();

    // Non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Movable
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Returns true if the socket owns a valid descriptor
    bool valid() const noexcept;

    // Returns the underlying file descriptor
    int fd() const noexcept;

    // Releases ownership without closing
    int release() noexcept;

private:
    int fd_;
};

} // namespace kv
