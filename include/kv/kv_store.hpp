#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <shared_mutex>


namespace kv {

/*
 * Thread-safe in-memory key–value store.
 * Defines the storage API.
 */
class KvStore {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;
    size_t size() const;

private:
    std::unordered_map<std::string, std::string> data_;
    mutable std::shared_mutex mutex_;
};

} // namespace kv
