#include "kv/kv_store.hpp"

namespace kv {

void KvStore::set(const std::string& key, const std::string& value) {
    data_[key] = value;
}

std::optional<std::string> KvStore::get(const std::string& key) const {
    auto it = data_.find(key);
    return it != data_.end() ? std::make_optional(it->second) : std::nullopt;
}

bool KvStore::del(const std::string& key) {
    return data_.erase(key) > 0;
}

bool KvStore::exists(const std::string& key) const {
    return data_.find(key) != data_.end();
}

size_t KvStore::size() const {
    return data_.size();
}

} // namespace kv
