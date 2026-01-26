#pragma once

#include "kv/socket.hpp"
#include <string>
#include <string_view>
#include <stdexcept>

namespace kv {

class IOError : public std::runtime_error {
public:
    explicit IOError(const std::string& msg) : std::runtime_error(msg) {}
};

/*
 * Represents a single client connection.
 */
class Connection {
public:
    Connection(Socket socket) : socket_(std::move(socket)) {};

    // read data sent by client
    std::string read_line();

    // write to client
    void write(std::string_view data);


private:
    Socket socket_;
};

} // namespace kv
