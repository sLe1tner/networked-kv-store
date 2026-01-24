#include "connection.hpp"
#include <unistd.h>
#include <stdexcept>

namespace kv {

std::string Connection::read_line() {
    std::string result;
    char ch;

    while (true) {
        ssize_t n = ::read(socket_.fd(), &ch, 1);

        if (n == 0)
            throw std::runtime_error("client disconnected");

        if (n < 0)
            throw std::runtime_error("read failed");

        if (ch == '\n')
            break;

        result.push_back(ch);
    }
    return result;
}

void Connection::write(std::string_view data) {
    const char* buf = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
        ssize_t n = ::write(socket_.fd(), buf, remaining);

        if (n < 0) {
            throw std::runtime_error("write failed");
        }

        buf += n;
        remaining -= n;
    }
}

} // namespace kv
