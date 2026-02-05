#include <gtest/gtest.h>
#include "kv/kv_store.hpp"
#include <thread>
#include <vector>

using namespace kv;

class KvStoreTest : public ::testing::Test {
protected:
    KvStore store;
};


TEST_F(KvStoreTest, SetAndGetData) {
    store.set("key", "value");
    EXPECT_TRUE(store.exists("key"));
    auto value = store.get("key");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, "value");
}

TEST_F(KvStoreTest, DeleteData) {
    store.set("key", "value");
    EXPECT_TRUE(store.del("key"));
    EXPECT_FALSE(store.get("key").has_value());
}

TEST_F(KvStoreTest, OverwriteData) {
    store.set("key", "old_value");
    store.set("key", "new_value");
    auto val = store.get("key");
    EXPECT_EQ(*val, "new_value");
}

TEST_F(KvStoreTest, GetNonExistentKeyReturnsNullopt) {
    auto val = store.get("missing_key");
    EXPECT_FALSE(val.has_value());
}

TEST_F(KvStoreTest, DeleteNonExistentKeyReturnsFalse) {
    EXPECT_FALSE(store.del("not_here"));
}

TEST_F(KvStoreTest, KeysAreCaseSensitive) {
    store.set("NAME", "ALICE");
    store.set("name", "bob");

    EXPECT_EQ(store.get("NAME"), "ALICE");
    EXPECT_EQ(store.get("name"), "bob");
}

TEST_F(KvStoreTest, HandlesLargeValues) {
    std::string big_data(1024 * 1024, 'A'); // 1MB value
    store.set("big", big_data);
    EXPECT_EQ(store.get("big"), big_data);
}

TEST_F(KvStoreTest, ConcurrentGetAndSet) {
    const int num_threads = 10;
    const int ops_per_thread = 100;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back([this, ops_per_thread, i]() {
            for (size_t j = 0; j < ops_per_thread; j++) {
                std::string id{std::to_string(i) + std::to_string(j)};
                std::string key{"thread_key_" + id};
                store.set(key, id + "value");
                EXPECT_EQ(store.get(key), id + "value");
            }
        });
    }
    for (auto& t : threads)
        t.join();

    EXPECT_EQ(store.size(), num_threads * ops_per_thread);
}
