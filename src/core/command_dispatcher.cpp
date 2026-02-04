
#include "kv/command_dispatcher.hpp"

namespace kv {

std::string CommandDispatcher::execute(const Command& command, KvStore& store) {
    return std::visit([&](const auto& cmd) -> std::string {
        using T = std::decay_t<decltype(cmd)>;

        if constexpr (std::is_same_v<T, Get>) {
            auto value = store.get(cmd.key);
            return value ?
                Protocol::format_value(*value) :
                Protocol::format_error("key not found");

        } else if constexpr (std::is_same_v<T, Set>) {
            store.set(cmd.key, cmd.value);
            return Protocol::format_ok();

        } else if constexpr (std::is_same_v<T, Del>) {
            bool success = store.del(cmd.key);
            return success ?
                Protocol::format_ok() :
                Protocol::format_error("key not found");
        } else if constexpr (std::is_same_v<T, Ping>) {
            return Protocol::format_value("Pong");
        }
    }, command);
}

} // namespace kv
