#include "connection.hpp"
#include <unistd.h>
#include <sys/socket.h>

namespace kv {


bool Connection::read_to_inbox() {
    char buffer[4096];
    ssize_t n = ::read(socket_.fd(), buffer, sizeof(buffer));

    if (n == 0)
        return false;
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true; // No data left to read
        throw IOError{"read failed"};
    }

    inbox_.append(buffer, n);
    return true;
}

std::optional<std::string> Connection::try_get_line() {
    auto pos = inbox_.find('\n');
    if (pos == std::string::npos) {
        return std::nullopt; // No full line yet
    }

    std::string line = inbox_.substr(0, pos);
    inbox_.erase(0, pos + 1); // Remove the line and the \n from the buffer
    return line;
}


void Connection::append_response(std::string data) {
    std::lock_guard lock(outbox_mutex_);
    outbox_.append(data);
}

bool Connection::has_pending_data() const {
    std::lock_guard lock(outbox_mutex_);
    return !outbox_.empty();
}

bool Connection::write_from_outbox() {
    std::lock_guard lock(outbox_mutex_);
    if (outbox_.empty())
        return false;

    // MSG_NOSIGNAL: don't SIGPIPE us if the socket is dead
    ssize_t n = ::send(socket_.fd(), outbox_.data(), outbox_.size(), MSG_NOSIGNAL);
    if (n > 0) {
        outbox_.erase(0, n); // Remove what was actually sent
    } else if (n < 0) {
        throw IOError("write failed");
    }
    return !outbox_.empty();
}


} // namespace kv
