#pragma once

#include "kv/kv_store.hpp"
#include "kv/protocol.hpp"

#include <string>

namespace kv {

class CommandDispatcher {
public:
    static std::string execute(const Command& command, KvStore& store);
};

} // namespace kv
