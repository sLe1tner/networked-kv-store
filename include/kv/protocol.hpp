#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <stdexcept>

namespace kv {

class ProtocolError : public std::runtime_error {
public:
    explicit ProtocolError(const std::string& msg) : std::runtime_error(msg) {}
};

struct Get {
    std::string key;
};

struct Set {
    std::string key;
    std::string value;
};

struct Del {
    std::string key;
};

using Command = std::variant<Get, Set, Del>;

/*
 * Parses and formats protocol messages.
 * Public-facing protocol logic.
 * Parse commands like SET, GET, DEL and return responses.
 */
class Protocol {
public:
    static Command parse(std::string_view line);

    static std::string format_ok();
    static std::string format_error(std::string_view message);
    static std::string format_value(std::string_view value);

private:
    static Command parse_tokens(const std::vector<std::string_view>& tokens);
};

} // namespace kv
