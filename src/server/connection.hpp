#pragma once

#include "kv/socket.hpp"
#include <string>
#include <string_view>

namespace kv {

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
