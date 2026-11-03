#pragma once
// In-memory fixed-window limiter for failed sign-in attempts (per process; resets on restart).
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace chronoplan::auth {

class RateLimiter {
public:
    using Clock = std::function<long long()>;   // unix seconds
    RateLimiter(int maxFailures, int windowSeconds, Clock clock);

    // True while `key` may still try (fewer than maxFailures failures in the current window).
    bool allowed(const std::string& key);
    void recordFailure(const std::string& key);
    void reset(const std::string& key);   // after a successful sign-in

private:
    struct Entry { long long windowStart = 0; int failures = 0; };
    void prune(long long now);   // caller holds mutex_

    int maxFailures_;
    int windowSeconds_;
    Clock clock_;
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace chronoplan::auth
