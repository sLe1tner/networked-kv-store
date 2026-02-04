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
    if (server_inbox_.size() + n > MAX_INBOX_SIZE) {
        server_inbox_.clear();
        throw BufferOverflowError{"value too large"};
    }

    server_inbox_.append(buffer, n);
    return true;
}

std::optional<std::string> Connection::try_get_line() {
    auto pos = server_inbox_.find('\n');
    if (pos == std::string::npos) {
        return std::nullopt; // No full line yet
    }

    std::string line = server_inbox_.substr(0, pos);
    server_inbox_.erase(0, pos + 1); // Remove the line and the \n from the buffer
    return line;
}


void Connection::append_response(std::string data) {
    std::lock_guard lock(outbox_mutex_);
    server_outbox_.append(data);
}

bool Connection::write_from_outbox() {
    std::lock_guard lock(outbox_mutex_);
    if (server_outbox_.empty())
        return false;

    // MSG_NOSIGNAL: don't SIGPIPE us if the socket is dead
    ssize_t n = ::send(socket_.fd(), server_outbox_.data(), server_outbox_.size(), MSG_NOSIGNAL);
    if (n >= 0) {
        server_outbox_.erase(0, n); // Remove what was actually sent
        return !server_outbox_.empty();
    }
    // n < 0
    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return true;
    throw IOError("write failed");
}



bool Connection::inbox_has_data() const {
    return !server_inbox_.empty();
}

bool Connection::outbox_has_data() const {
    std::lock_guard lock(outbox_mutex_);
    return !server_outbox_.empty();
}

} // namespace kv
