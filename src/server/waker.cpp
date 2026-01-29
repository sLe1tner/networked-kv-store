
#include "waker.hpp"


namespace kv {

Waker::Waker() {
    if (pipe(pipe_fds_) == -1)
        throw std::runtime_error("Failed to create self-pipe");
    // Set both ends to non-blocking
    fcntl(pipe_fds_[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_fds_[1], F_SETFL, O_NONBLOCK);
}

Waker::~Waker() {
    close(pipe_fds_[0]);
    close(pipe_fds_[1]);
}

int Waker::read_fd() const {
    return pipe_fds_[0];
}

void Waker::notify() {
    char c = 'x';
    write(pipe_fds_[1], &c, 1);
}

void Waker::clear() {
    char buf[16];
    while (read(pipe_fds_[0], buf, sizeof(buf)) > 0);
}

} // namespace kv
