#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>


namespace kv {

class Waker {
public:
    Waker();
    ~Waker();
    int read_fd() const;

    // This is the self-pipe trigger called by the signal handler
    void notify();

    void clear();

private:
    int pipe_fds_[2];
};

} // namespace kv
