
#include "kv/socket.hpp"

#include <unistd.h> // close()


namespace kv {


Socket::Socket() noexcept: fd_(-1) {}

Socket::Socket(int fd) noexcept: fd_(fd) {}

Socket::~Socket() {
    if (fd_ != -1) {
        ::close(fd_);
    }
}

Socket::Socket(Socket&& other) noexcept: fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (fd_ != -1) {
            ::close(fd_);
        }
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool Socket::valid() const noexcept {
    return fd_ != -1;
}

int Socket::fd() const noexcept {
    return fd_;
}

int Socket::release() noexcept {
    int tmp = fd_;
    fd_ = -1;
    return tmp;
}

} // namespace kv
