#include "kv/kv_store.hpp"

namespace kv {

bool KvStore::set(const std::string& key, const std::string& value) {
    return false;
}

std::optional<std::string> KvStore::get(const std::string& key) const {
    return std::nullopt;
}

bool KvStore::del(const std::string& key) {
    return false;
}

} // namespace kv
