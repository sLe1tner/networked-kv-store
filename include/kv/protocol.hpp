#pragma once

#include <string>

namespace kv {

/*
 * Parses and formats protocol messages.
 * Public-facing protocol logic.
 * Parse commands like SET, GET, DEL and return responses.
 */
class Protocol {
    static std::string handleCommand(const std::string& input);
};

} // namespace kv
