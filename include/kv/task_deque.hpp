#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace kv {

template <typename T>
class TaskDeque {
public:
    // Push a new task into the deque
    // Reactor calls this to drop off a task
    void push_back(T task) {
        {
            std::lock_guard lock(deque_mutex_);
            deque_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

    // Pop a task (blocks if the deque is empty)
    // Workers call this to grab work
    // Returns std::nullopt if the thread is requested to stop
    std::optional<T> wait_and_pop_front(std::stop_token stop_token) {
        std::unique_lock lock(deque_mutex_);
        // C++20 wait: stays asleep until data exists OR stop is requested
        bool success = cv_.wait(lock, stop_token, [this]() {
            return !deque_.empty();
        });

        if (!success || deque_.empty()) {
            return std::nullopt;
        }

        T task = std::move(deque_.front());
        deque_.pop_front();
        return task;
    }

    bool empty() const {
        std::lock_guard lock(deque_mutex_);
        return deque_.empty();
    }

private:
    std::deque<T> deque_;
    mutable std::mutex deque_mutex_;
    std::condition_variable_any cv_;
};

} // namespace kv
