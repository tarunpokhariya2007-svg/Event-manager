#pragma once
// Vector-backed binary heap with an id index, giving O(log n) update/remove by key.
//
// Semantics : Compare(a, b) == true means `a` must be extracted BEFORE `b`
//             (std::less<T> -> min-heap, std::greater<T> -> max-heap).
//             The comparator should be a strict weak order; give it a unique tie-breaker
//             (e.g. an id) so extraction order is fully deterministic.
// Keys      : KeyOf(const T&) -> std::string uniquely identifies an entry.
// Duplicates: push() of an existing key is REJECTED (returns false, queue unchanged);
//             use update() to change an entry. assign() throws std::invalid_argument.
//
// Complexity (n = size): push O(log n) | peek/top O(1) | pop O(log n) |
//   update O(log n) | remove O(log n) | contains/find O(1) average |
//   assign (heap construction, Floyd) O(n) | clear O(n).
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chronoplan::algorithms {

template <typename T, typename Compare = std::less<T>, typename KeyOf = std::function<std::string(const T&)>>
class PriorityQueue {
public:
    explicit PriorityQueue(Compare cmp = Compare(), KeyOf key = KeyOf())
        : cmp_(std::move(cmp)), key_(std::move(key)) {}

    bool empty() const noexcept { return heap_.empty(); }
    size_t size() const noexcept { return heap_.size(); }

    void clear() {
        heap_.clear();
        index_.clear();
    }

    // False (and no change) when an entry with the same key exists.
    bool push(T value) {
        std::string k = key_(value);
        if (index_.find(k) != index_.end()) return false;
        heap_.push_back(std::move(value));
        index_.emplace(std::move(k), heap_.size() - 1);
        siftUp(heap_.size() - 1);
        return true;
    }

    // nullptr when empty.
    const T* peek() const noexcept { return heap_.empty() ? nullptr : &heap_.front(); }

    // Throws std::out_of_range when empty.
    const T& top() const {
        if (heap_.empty()) throw std::out_of_range("PriorityQueue::top on empty queue");
        return heap_.front();
    }

    // Removes and returns the highest-ranked entry; std::nullopt when empty.
    std::optional<T> pop() {
        if (heap_.empty()) return std::nullopt;
        return removeAt(0);
    }

    bool contains(const std::string& key) const { return index_.find(key) != index_.end(); }

    const T* find(const std::string& key) const {
        const auto it = index_.find(key);
        return it == index_.end() ? nullptr : &heap_[it->second];
    }

    // Replaces the entry with the same key as `value` and restores heap order.
    // False if no such entry exists.
    bool update(T value) {
        const auto it = index_.find(key_(value));
        if (it == index_.end()) return false;
        const size_t i = it->second;
        heap_[i] = std::move(value);
        restore(i);
        return true;
    }

    // Removes the entry with `key`. False if absent.
    bool remove(const std::string& key) {
        const auto it = index_.find(key);
        if (it == index_.end()) return false;
        removeAt(it->second);
        return true;
    }

    // Replaces the contents with `items` using bottom-up heap construction, O(n).
    // Throws std::invalid_argument on duplicate keys (queue left unchanged).
    void assign(std::vector<T> items) {
        std::unordered_map<std::string, size_t> idx;
        idx.reserve(items.size());
        for (size_t i = 0; i < items.size(); ++i)
            if (!idx.emplace(key_(items[i]), i).second)
                throw std::invalid_argument("PriorityQueue::assign: duplicate key");
        heap_ = std::move(items);
        index_ = std::move(idx);
        for (size_t i = heap_.size() / 2; i-- > 0;) siftDown(i);
    }

    // Test/debug aid: heap property for every parent/child pair and a consistent index.
    bool isValidHeap() const {
        if (index_.size() != heap_.size()) return false;
        for (size_t i = 0; i < heap_.size(); ++i) {
            const auto it = index_.find(key_(heap_[i]));
            if (it == index_.end() || it->second != i) return false;
            if (i > 0 && cmp_(heap_[i], heap_[(i - 1) / 2])) return false;
        }
        return true;
    }

private:
    void swapNodes(size_t i, size_t j) {
        std::swap(heap_[i], heap_[j]);
        index_[key_(heap_[i])] = i;
        index_[key_(heap_[j])] = j;
    }

    size_t siftUp(size_t i) {
        while (i > 0) {
            const size_t p = (i - 1) / 2;
            if (!cmp_(heap_[i], heap_[p])) break;
            swapNodes(i, p);
            i = p;
        }
        return i;
    }

    void siftDown(size_t i) {
        const size_t n = heap_.size();
        for (;;) {
            const size_t l = 2 * i + 1, r = l + 1;
            size_t best = i;
            if (l < n && cmp_(heap_[l], heap_[best])) best = l;
            if (r < n && cmp_(heap_[r], heap_[best])) best = r;
            if (best == i) return;
            swapNodes(i, best);
            i = best;
        }
    }

    void restore(size_t i) { siftDown(siftUp(i)); }

    T removeAt(size_t i) {
        index_.erase(key_(heap_[i]));          // key before the value is moved from
        T out = std::move(heap_[i]);
        const size_t last = heap_.size() - 1;
        if (i != last) {
            heap_[i] = std::move(heap_[last]);
            heap_.pop_back();
            index_[key_(heap_[i])] = i;
            restore(i);
        } else {
            heap_.pop_back();
        }
        return out;
    }

    std::vector<T> heap_;
    std::unordered_map<std::string, size_t> index_;
    Compare cmp_;
    KeyOf key_;
};

}  // namespace chronoplan::algorithms
