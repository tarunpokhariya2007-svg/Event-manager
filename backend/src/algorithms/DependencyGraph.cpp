#include "algorithms/DependencyGraph.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include "algorithms/IdOrdering.hpp"
#include "algorithms/PriorityQueue.hpp"

namespace chronoplan::algorithms {

namespace {
void eraseKey(std::vector<std::string>& v, const std::string& key) {
    const auto it = std::find(v.begin(), v.end(), key);
    if (it != v.end()) v.erase(it);
}
std::string describe(const models::Id& id) { return id.numeric ? id.value : "\"" + id.value + "\""; }
}  // namespace

const DependencyGraph::Vertex& DependencyGraph::require(const models::Id& id) const {
    const auto it = adj_.find(id.key());
    if (it == adj_.end()) throw MissingVertexError("vertex " + describe(id) + " does not exist");
    return it->second;
}

std::vector<models::Id> DependencyGraph::idsOf(const std::vector<std::string>& keys) const {
    std::vector<models::Id> out;
    out.reserve(keys.size());
    for (const std::string& k : keys) out.push_back(adj_.at(k).id);
    return out;
}

bool DependencyGraph::addVertex(const models::Id& id) {
    if (id.empty()) throw GraphError("vertex id must not be empty");
    Vertex v;
    v.id = id;
    return adj_.emplace(id.key(), std::move(v)).second;
}

bool DependencyGraph::removeVertex(const models::Id& id) {
    const std::string key = id.key();
    const auto it = adj_.find(key);
    if (it == adj_.end()) return false;
    for (const std::string& p : it->second.in) eraseKey(adj_.at(p).out, key);
    for (const std::string& d : it->second.out) eraseKey(adj_.at(d).in, key);
    edges_ -= it->second.in.size() + it->second.out.size();
    adj_.erase(it);
    return true;
}

bool DependencyGraph::hasVertex(const models::Id& id) const { return adj_.find(id.key()) != adj_.end(); }

void DependencyGraph::addEdge(const models::Id& prerequisite, const models::Id& dependent) {
    if (prerequisite == dependent)
        throw SelfEdgeError("task " + describe(prerequisite) + " cannot depend on itself");
    const auto p = adj_.find(prerequisite.key());
    if (p == adj_.end()) throw MissingVertexError("vertex " + describe(prerequisite) + " does not exist");
    const auto d = adj_.find(dependent.key());
    if (d == adj_.end()) throw MissingVertexError("vertex " + describe(dependent) + " does not exist");
    const std::string& dk = d->first;
    if (std::find(p->second.out.begin(), p->second.out.end(), dk) != p->second.out.end())
        throw DuplicateEdgeError("edge " + describe(prerequisite) + " -> " + describe(dependent) +
                                 " already exists");
    p->second.out.push_back(dk);
    d->second.in.push_back(p->first);
    ++edges_;
}

bool DependencyGraph::removeEdge(const models::Id& prerequisite, const models::Id& dependent) {
    const auto p = adj_.find(prerequisite.key());
    const auto d = adj_.find(dependent.key());
    if (p == adj_.end() || d == adj_.end()) return false;
    auto& out = p->second.out;
    const auto it = std::find(out.begin(), out.end(), d->first);
    if (it == out.end()) return false;
    out.erase(it);
    eraseKey(d->second.in, p->first);
    --edges_;
    return true;
}

bool DependencyGraph::hasEdge(const models::Id& prerequisite, const models::Id& dependent) const {
    const auto p = adj_.find(prerequisite.key());
    if (p == adj_.end()) return false;
    const std::string dk = dependent.key();
    return std::find(p->second.out.begin(), p->second.out.end(), dk) != p->second.out.end();
}

std::vector<models::Id> DependencyGraph::getPrerequisites(const models::Id& id) const {
    return idsOf(require(id).in);
}

std::vector<models::Id> DependencyGraph::getDependents(const models::Id& id) const {
    return idsOf(require(id).out);
}

bool DependencyGraph::wouldCreateCycle(const models::Id& prerequisite, const models::Id& dependent) const {
    if (prerequisite == dependent) return true;
    // A cycle appears iff `prerequisite` is already reachable from `dependent`.
    const Vertex& start = require(dependent);
    const std::string target = prerequisite.key();
    require(prerequisite);
    std::unordered_set<std::string> seen{start.id.key()};
    std::vector<const Vertex*> stack{&start};
    while (!stack.empty()) {
        const Vertex* v = stack.back();
        stack.pop_back();
        for (const std::string& k : v->out) {
            if (k == target) return true;
            if (seen.insert(k).second) stack.push_back(&adj_.at(k));
        }
    }
    return false;
}

std::vector<models::Id> DependencyGraph::vertices() const {
    std::vector<models::Id> out;
    out.reserve(adj_.size());
    for (const auto& kv : adj_) out.push_back(kv.second.id);
    std::sort(out.begin(), out.end(), IdLess{});
    return out;
}

void DependencyGraph::clear() noexcept {
    adj_.clear();
    edges_ = 0;
}

std::optional<std::vector<models::Id>> DependencyGraph::findCycle() const {
    // Iterative three-colour DFS (0 white, 1 on the current path, 2 finished): no recursion,
    // so arbitrarily long chains cannot overflow the call stack.
    struct Frame { const Vertex* v; size_t next; };
    std::unordered_map<std::string, std::uint8_t> color;
    color.reserve(adj_.size());
    for (const models::Id& root : vertices()) {
        const std::string rk = root.key();
        if (color[rk] != 0) continue;
        color[rk] = 1;
        std::vector<Frame> stack{{&adj_.at(rk), 0}};
        while (!stack.empty()) {
            Frame& f = stack.back();
            if (f.next < f.v->out.size()) {
                const std::string& nk = f.v->out[f.next++];
                std::uint8_t& c = color[nk];
                if (c == 0) {
                    c = 1;
                    stack.push_back({&adj_.at(nk), 0});
                } else if (c == 1) {
                    const Vertex* target = &adj_.at(nk);
                    size_t pos = stack.size();
                    while (pos > 0 && stack[pos - 1].v != target) --pos;
                    std::vector<models::Id> cycle;
                    for (size_t i = pos - 1; i < stack.size(); ++i) cycle.push_back(stack[i].v->id);
                    return cycle;
                }
            } else {
                color[f.v->id.key()] = 2;
                stack.pop_back();
            }
        }
    }
    return std::nullopt;
}

TopologicalSortResult DependencyGraph::topologicalSort() const {
    TopologicalSortResult result;
    std::unordered_map<std::string, size_t> indegree;
    indegree.reserve(adj_.size());
    std::vector<models::Id> zero;
    for (const auto& [k, v] : adj_) {
        indegree[k] = v.in.size();
        if (v.in.empty()) zero.push_back(v.id);
    }
    PriorityQueue<models::Id, IdLess, IdKey> ready;
    ready.assign(std::move(zero));          // O(V) heap construction
    result.order.reserve(adj_.size());
    while (auto next = ready.pop()) {
        for (const std::string& dk : adj_.at(next->key()).out)
            if (--indegree[dk] == 0) ready.push(adj_.at(dk).id);
        result.order.push_back(std::move(*next));
    }
    result.success = (result.order.size() == adj_.size());
    if (!result.success) {
        for (const auto& [k, v] : adj_)
            if (indegree[k] > 0) result.unresolved.push_back(v.id);
        std::sort(result.unresolved.begin(), result.unresolved.end(), IdLess{});
    }
    return result;
}

std::vector<models::Id> DependencyGraph::topologicalOrder() const {
    TopologicalSortResult r = topologicalSort();
    if (!r.success) throw CycleError("dependency graph contains a cycle; no topological order exists");
    return std::move(r.order);
}

std::vector<models::Id> DependencyGraph::readyTasks(const std::vector<models::Id>& completed) const {
    std::unordered_set<std::string> done;
    done.reserve(completed.size());
    for (const models::Id& c : completed) done.insert(c.key());
    std::vector<models::Id> out;
    for (const auto& [k, v] : adj_) {
        if (done.count(k)) continue;
        const bool ok = std::all_of(v.in.begin(), v.in.end(),
                                    [&](const std::string& p) { return done.count(p) > 0; });
        if (ok) out.push_back(v.id);
    }
    std::sort(out.begin(), out.end(), IdLess{});
    return out;
}

std::vector<std::vector<models::Id>> DependencyGraph::cyclicComponents() const {
    // Iterative Tarjan SCC (no recursion: long chains cannot overflow the call stack).
    struct Frame { const Vertex* v; size_t next; };
    std::unordered_map<std::string, int> index, low;
    std::unordered_set<std::string> onStack;
    std::vector<const Vertex*> sccStack;
    std::vector<std::vector<models::Id>> result;
    int counter = 0;
    for (const models::Id& root : vertices()) {
        const std::string rk = root.key();
        if (index.count(rk)) continue;
        index[rk] = low[rk] = counter++;
        sccStack.push_back(&adj_.at(rk));
        onStack.insert(rk);
        std::vector<Frame> call{{&adj_.at(rk), 0}};
        while (!call.empty()) {
            const Vertex* v = call.back().v;
            const std::string vk = v->id.key();
            if (call.back().next < v->out.size()) {
                const std::string wk = v->out[call.back().next++];
                if (!index.count(wk)) {
                    index[wk] = low[wk] = counter++;
                    sccStack.push_back(&adj_.at(wk));
                    onStack.insert(wk);
                    call.push_back({&adj_.at(wk), 0});
                } else if (onStack.count(wk)) {
                    low[vk] = (std::min)(low[vk], index[wk]);
                }
            } else {
                if (low[vk] == index[vk]) {
                    std::vector<models::Id> comp;
                    for (;;) {
                        const Vertex* w = sccStack.back();
                        sccStack.pop_back();
                        onStack.erase(w->id.key());
                        comp.push_back(w->id);
                        if (w == v) break;
                    }
                    if (comp.size() > 1) {
                        std::sort(comp.begin(), comp.end(), IdLess{});
                        result.push_back(std::move(comp));
                    }
                }
                call.pop_back();
                if (!call.empty()) {
                    const std::string pk = call.back().v->id.key();
                    low[pk] = (std::min)(low[pk], low[vk]);
                }
            }
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return compareIds(a.front(), b.front()) < 0; });
    return result;
}

std::vector<models::Id> DependencyGraph::reachableFrom(const std::vector<models::Id>& seeds) const {
    std::unordered_set<std::string> seen;
    std::vector<const Vertex*> stack;
    for (const models::Id& s : seeds) {
        const auto it = adj_.find(s.key());
        if (it != adj_.end() && seen.insert(it->first).second) stack.push_back(&it->second);
    }
    std::vector<models::Id> out;
    while (!stack.empty()) {
        const Vertex* v = stack.back();
        stack.pop_back();
        for (const std::string& k : v->out)
            if (seen.insert(k).second) {
                const Vertex& w = adj_.at(k);
                out.push_back(w.id);
                stack.push_back(&w);
            }
    }
    std::sort(out.begin(), out.end(), IdLess{});
    return out;
}

std::vector<models::Id> DependencyGraph::cycleAffectedVertices() const {
    std::vector<models::Id> members;
    for (const auto& comp : cyclicComponents()) members.insert(members.end(), comp.begin(), comp.end());
    std::vector<models::Id> out = members;
    for (models::Id& d : reachableFrom(members)) out.push_back(std::move(d));
    std::sort(out.begin(), out.end(), IdLess{});
    return out;
}

std::vector<models::Id> DependencyGraph::readyTasksExcludingCycles(
    const std::vector<models::Id>& completed) const {
    std::unordered_set<std::string> blocked;
    for (const models::Id& id : cycleAffectedVertices()) blocked.insert(id.key());
    std::vector<models::Id> out;
    for (models::Id& id : readyTasks(completed))
        if (!blocked.count(id.key())) out.push_back(std::move(id));
    return out;
}

}  // namespace chronoplan::algorithms
