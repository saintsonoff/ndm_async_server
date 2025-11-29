#pragma once

// stlcpp
#include <atomic>
#include <concepts>
#include <string>
#include <string_view>

namespace async_server {

template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

struct Statistics {
   public:
    void increment_total() {
        total_clients_.fetch_add(1, std::memory_order_relaxed);
    }
    void increment_current() {
        current_clients_.fetch_add(1, std::memory_order_relaxed);
    }
    void decrement_current() {
        current_clients_.fetch_sub(1, std::memory_order_relaxed);
    }

    int get_total() const {
        return total_clients_.load(std::memory_order_relaxed);
    }
    int get_current() const {
        return current_clients_.load(std::memory_order_relaxed);
    }

   private:
    std::atomic<int> total_clients_{0};
    std::atomic<int> current_clients_{0};
};

class CommandProcessor {
   public:
    explicit CommandProcessor(Statistics& stats,
                              std::atomic<bool>& shutdown_flag)
        : stats_(stats), should_shutdown_(shutdown_flag) {}

    std::string process(const std::string& message);

    template <StringLike T>
    std::string process(const T& message) {
        return process(std::string(message));
    }

   private:
    std::string get_time() const;
    std::string get_stats() const;

    Statistics& stats_;
    std::atomic<bool>& should_shutdown_;
};

}  // namespace async_server
