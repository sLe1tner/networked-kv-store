#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace kv {

enum class CommandType {
    Get,
    Set,
    Del
};

struct Command {
    CommandType type;
    std::string key;
    std::string value;
};

/*
 * Parses and formats protocol messages.
 * Public-facing protocol logic.
 * Parse commands like SET, GET, DEL and return responses.
 */
class Protocol {
public:
    Command parse(std::string_view line) const;

    std::string format_ok() const;
    std::string format_error(std::string_view message) const;
    std::string format_value(std::string_view value) const;

private:
    Command parse_tokens(const std::vector<std::string_view>& tokens) const;
};

} // namespace kv
