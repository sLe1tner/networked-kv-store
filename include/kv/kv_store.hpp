#pragma once

#include <optional>
#include <string>


namespace kv {

/*
 * Thread-safe in-memory key–value store.
 * Defines the storage API.
 */
class KvStore {
public:
    bool set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
};

} // namespace kv
