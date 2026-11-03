#include "algorithms/IntervalTree.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace chronoplan::algorithms {

namespace detail {
struct IntervalNode {
    TimeInterval interval;
    std::unique_ptr<IntervalNode> left;
    std::unique_ptr<IntervalNode> right;
    int height = 1;
    int maxEnd;
    explicit IntervalNode(TimeInterval iv) : interval(std::move(iv)), maxEnd(interval.end) {}
};
}  // namespace detail

namespace {

using detail::IntervalNode;
using NodePtr = std::unique_ptr<IntervalNode>;

int heightOf(const NodePtr& n) { return n ? n->height : 0; }
int maxEndOf(const NodePtr& n) { return n ? n->maxEnd : std::numeric_limits<int>::min(); }
int balanceOf(const NodePtr& n) { return n ? heightOf(n->left) - heightOf(n->right) : 0; }

void update(IntervalNode& n) {
    n.height = 1 + std::max(heightOf(n.left), heightOf(n.right));
    n.maxEnd = std::max({n.interval.end, maxEndOf(n.left), maxEndOf(n.right)});
}

NodePtr rotateRight(NodePtr y) {
    NodePtr x = std::move(y->left);
    y->left = std::move(x->right);
    update(*y);
    x->right = std::move(y);
    update(*x);
    return x;
}

NodePtr rotateLeft(NodePtr x) {
    NodePtr y = std::move(x->right);
    x->right = std::move(y->left);
    update(*x);
    y->left = std::move(x);
    update(*y);
    return y;
}

NodePtr rebalance(NodePtr n) {
    update(*n);
    const int bf = balanceOf(n);
    if (bf > 1) {
        if (balanceOf(n->left) < 0) n->left = rotateLeft(std::move(n->left));
        return rotateRight(std::move(n));
    }
    if (bf < -1) {
        if (balanceOf(n->right) > 0) n->right = rotateRight(std::move(n->right));
        return rotateLeft(std::move(n));
    }
    return n;
}

NodePtr insertNode(NodePtr n, TimeInterval&& iv, bool& inserted) {
    if (!n) {
        inserted = true;
        return std::make_unique<IntervalNode>(std::move(iv));
    }
    const int c = compareIntervals(iv, n->interval);
    if (c == 0) {
        inserted = false;
        return n;
    }
    if (c < 0) n->left = insertNode(std::move(n->left), std::move(iv), inserted);
    else n->right = insertNode(std::move(n->right), std::move(iv), inserted);
    return inserted ? rebalance(std::move(n)) : std::move(n);
}

// Detaches the minimum node of the subtree, moving its interval into `out`.
NodePtr removeMin(NodePtr n, TimeInterval& out) {
    if (!n->left) {
        out = std::move(n->interval);
        return std::move(n->right);
    }
    n->left = removeMin(std::move(n->left), out);
    return rebalance(std::move(n));
}

NodePtr eraseNode(NodePtr n, const TimeInterval& key, bool& removed) {
    if (!n) return nullptr;
    const int c = compareIntervals(key, n->interval);
    if (c < 0) {
        n->left = eraseNode(std::move(n->left), key, removed);
    } else if (c > 0) {
        n->right = eraseNode(std::move(n->right), key, removed);
    } else {
        removed = true;
        if (!n->left) return std::move(n->right);
        if (!n->right) return std::move(n->left);
        TimeInterval successor;
        n->right = removeMin(std::move(n->right), successor);
        n->interval = std::move(successor);
    }
    return rebalance(std::move(n));
}

void collectOverlaps(const IntervalNode* n, int s, int e, std::vector<TimeInterval>& out) {
    // Nothing in this subtree ends after s => nothing can overlap.
    if (!n || n->maxEnd <= s) return;
    collectOverlaps(n->left.get(), s, e, out);
    // This node and its whole right subtree start at or after n.start >= e.
    if (n->interval.start >= e) return;
    if (n->interval.overlaps(s, e)) out.push_back(n->interval);
    collectOverlaps(n->right.get(), s, e, out);
}

void inorder(const IntervalNode* n, std::vector<TimeInterval>& out) {
    if (!n) return;
    inorder(n->left.get(), out);
    out.push_back(n->interval);
    inorder(n->right.get(), out);
}

bool checkNode(const IntervalNode* n, const TimeInterval*& prev, size_t& count, std::string& why) {
    if (!n) return true;
    if (!checkNode(n->left.get(), prev, count, why)) return false;
    if (prev && compareIntervals(*prev, n->interval) >= 0) {
        why = "BST ordering violated";
        return false;
    }
    prev = &n->interval;
    ++count;
    if (!checkNode(n->right.get(), prev, count, why)) return false;
    if (n->height != 1 + std::max(heightOf(n->left), heightOf(n->right))) {
        why = "stored height is wrong";
        return false;
    }
    if (std::abs(heightOf(n->left) - heightOf(n->right)) > 1) {
        why = "AVL balance violated";
        return false;
    }
    if (n->maxEnd != std::max({n->interval.end, maxEndOf(n->left), maxEndOf(n->right)})) {
        why = "maxEnd augmentation is wrong";
        return false;
    }
    return true;
}

void requireRange(int s, int e) {
    if (!TimeInterval::isValidRange(s, e))
        throw models::ValidationError("range start (" + std::to_string(s) + ") must be before end (" +
                                      std::to_string(e) + ")");
}

}  // namespace

std::vector<TimeSlot> computeFreeSlots(const std::vector<TimeInterval>& occupied, int windowStart,
                                       int windowEnd, int minLength) {
    if (windowStart >= windowEnd)
        throw models::ValidationError("window start must be before window end");
    if (minLength < 1) throw models::ValidationError("minLength must be at least 1");

    std::vector<std::pair<int, int>> clipped;
    clipped.reserve(occupied.size());
    for (const TimeInterval& iv : occupied) {
        if (iv.start >= iv.end) continue;                       // zero/negative length: ignore
        if (iv.end <= windowStart || iv.start >= windowEnd) continue;   // outside (or only touching)
        clipped.emplace_back((std::max)(iv.start, windowStart), (std::min)(iv.end, windowEnd));
    }
    std::sort(clipped.begin(), clipped.end());

    std::vector<TimeSlot> free;
    int cursor = windowStart;
    for (const auto& [s, e] : clipped) {
        if (s > cursor && s - cursor >= minLength) free.push_back({cursor, s});
        cursor = (std::max)(cursor, e);
    }
    if (windowEnd > cursor && windowEnd - cursor >= minLength) free.push_back({cursor, windowEnd});
    return free;
}

IntervalTree::IntervalTree() = default;
IntervalTree::~IntervalTree() = default;
IntervalTree::IntervalTree(IntervalTree&& o) noexcept : root_(std::move(o.root_)), size_(o.size_) { o.size_ = 0; }
IntervalTree& IntervalTree::operator=(IntervalTree&& o) noexcept {
    if (this != &o) {
        root_ = std::move(o.root_);
        size_ = o.size_;
        o.size_ = 0;
    }
    return *this;
}

bool IntervalTree::insert(TimeInterval interval) {
    interval.validate();
    bool inserted = false;
    root_ = insertNode(std::move(root_), std::move(interval), inserted);
    if (inserted) ++size_;
    return inserted;
}

bool IntervalTree::remove(const TimeInterval& interval) {
    bool removed = false;
    root_ = eraseNode(std::move(root_), interval, removed);
    if (removed) --size_;
    return removed;
}

bool IntervalTree::contains(const TimeInterval& interval) const {
    const IntervalNode* n = root_.get();
    while (n) {
        const int c = compareIntervals(interval, n->interval);
        if (c == 0) return true;
        n = c < 0 ? n->left.get() : n->right.get();
    }
    return false;
}

std::optional<TimeInterval> IntervalTree::findAnyOverlap(int start, int end) const {
    requireRange(start, end);
    const IntervalNode* n = root_.get();
    while (n) {
        if (n->interval.overlaps(start, end)) return n->interval;
        // If the left subtree holds nothing ending after `start`, only the right can match.
        // If it does but nothing there overlaps, the right subtree cannot overlap either.
        n = (n->left && n->left->maxEnd > start) ? n->left.get() : n->right.get();
    }
    return std::nullopt;
}

std::optional<TimeInterval> IntervalTree::findAnyOverlap(const TimeInterval& r) const {
    return findAnyOverlap(r.start, r.end);
}

std::vector<TimeInterval> IntervalTree::findAllOverlaps(int start, int end) const {
    requireRange(start, end);
    std::vector<TimeInterval> out;
    collectOverlaps(root_.get(), start, end, out);
    return out;
}

std::vector<TimeInterval> IntervalTree::findAllOverlaps(const TimeInterval& r) const {
    return findAllOverlaps(r.start, r.end);
}

std::vector<TimeSlot> IntervalTree::findFreeSlots(int windowStart, int windowEnd, int minLength) const {
    requireRange(windowStart, windowEnd);
    return computeFreeSlots(findAllOverlaps(windowStart, windowEnd), windowStart, windowEnd, minLength);
}

std::vector<TimeInterval> IntervalTree::toVector() const {
    std::vector<TimeInterval> out;
    out.reserve(size_);
    inorder(root_.get(), out);
    return out;
}

int IntervalTree::height() const noexcept { return heightOf(root_); }

std::optional<int> IntervalTree::maxEnd() const noexcept {
    if (!root_) return std::nullopt;
    return root_->maxEnd;
}

void IntervalTree::clear() noexcept {
    root_.reset();
    size_ = 0;
}

bool IntervalTree::checkInvariants(std::string* reason) const {
    std::string why;
    const TimeInterval* prev = nullptr;
    size_t count = 0;
    bool ok = checkNode(root_.get(), prev, count, why);
    if (ok && count != size_) {
        why = "size mismatch";
        ok = false;
    }
    if (!ok && reason) *reason = why;
    return ok;
}

}  // namespace chronoplan::algorithms
