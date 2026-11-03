#include "auth/RateLimiter.hpp"

namespace chronoplan::auth {

RateLimiter::RateLimiter(int maxFailures, int windowSeconds, Clock clock)
    : maxFailures_(maxFailures), windowSeconds_(windowSeconds), clock_(std::move(clock)) {}

void RateLimiter::prune(long long now) {
    if (entries_.size() < 4096) return;   // only sweep when the table has grown
    for (auto it = entries_.begin(); it != entries_.end();)
        it = (now - it->second.windowStart >= windowSeconds_) ? entries_.erase(it) : std::next(it);
}

bool RateLimiter::allowed(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = entries_.find(key);
    if (it == entries_.end()) return true;
    if (clock_() - it->second.windowStart >= windowSeconds_) { entries_.erase(it); return true; }
    return it->second.failures < maxFailures_;
}

void RateLimiter::recordFailure(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const long long now = clock_();
    prune(now);
    Entry& e = entries_[key];
    if (e.failures == 0 || now - e.windowStart >= windowSeconds_) { e.windowStart = now; e.failures = 0; }
    ++e.failures;
}

void RateLimiter::reset(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.erase(key);
}

}  // namespace chronoplan::auth
