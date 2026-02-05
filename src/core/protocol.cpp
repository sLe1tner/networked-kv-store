#include "kv/protocol.hpp"
#include <stdexcept>

namespace kv {


Command Protocol::parse(std::string_view line) {
    // CRLF tolerance (windows, telnet, netcat)
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }

    std::vector<std::string_view> tokens;
    tokens.reserve(3);

    size_t pos = 0;

    while (pos < line.size()) {
        // Skip spaces
        while (pos < line.size() && line[pos] == ' ')
            ++pos;

        if (pos >= line.size())
            break;

        size_t start = pos;
        while (pos < line.size() && line[pos] != ' ')
            ++pos;

        tokens.emplace_back(line.substr(start, pos - start));
    }

    if (tokens.empty()) {
        return NoOp{ };
    }

    return parse_tokens(tokens);
}

Command Protocol::parse_tokens(const std::vector<std::string_view>& tokens) {
    std::string cmd{tokens[0]};
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    if (cmd == "get") {
        if (tokens.size() != 2)
            throw ProtocolError{"GET requires exactly one argument"};

        return Get{ std::string{tokens[1]} };
    }

    if (cmd == "set") {
        if (tokens.size() != 3)
            throw ProtocolError{"SET requires exactly two arguments"};

        return Set{
            std::string{tokens[1]},
            std::string{tokens[2]}
        };
    }

    if (cmd == "del") {
        if (tokens.size() != 2)
            throw ProtocolError{"DEL requires exactly one argument"};

        return Del{ std::string{tokens[1]} };
    }

    if (cmd == "ping") {
        if (tokens.size() != 1)
            throw ProtocolError{"PING requires exactly zero argument"};
        return Ping{ };
    }

    throw ProtocolError{"unknown command"};
}

std::string Protocol::format_ok() {
    return "+OK\n";
}

std::string Protocol::format_error(std::string_view message) {
    return "-ERR " + std::string{message} + "\n";
}

std::string Protocol::format_value(std::string_view value) {
    return "$" + std::string{value} + "\n";
}

} // namespace kv
