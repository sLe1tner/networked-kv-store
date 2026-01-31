#pragma once

#include "kv/socket.hpp"
#include <string>
#include <string_view>
#include <stdexcept>
#include <mutex>
#include <optional>

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

    // append response to outbox
    void append_response(std::string data);

    bool has_pending_data() const;

    // Write to client. Return true if there is still data left to send
    bool write_from_outbox();

    // returns false if client disconnected
    bool read_to_inbox();

    // return line if we have a full one (ends in \n)
    std::optional<std::string> try_get_line();


private:
    Socket socket_;
    std::string outbox_;
    mutable std::mutex outbox_mutex_;

    std::string inbox_;
};

} // namespace kv
