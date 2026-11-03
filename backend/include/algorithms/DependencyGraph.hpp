#pragma once
// In-memory dependency graph (adjacency lists) over type-safe task ids.
//
// Edge direction:  A -> B  means "A must be completed BEFORE B"
//                  (A is a prerequisite of B; B is a dependent of A).
// So for the chain A -> B -> C, getPrerequisites(C) = {B} and getDependents(A) = {B}.
// Note the Phase 2 repository's Dependency{task, prerequisite} maps to addEdge(prerequisite, task).
//
// Ids keep their type: numeric 7 and string "7" are different vertices.
// Deterministic order: vertices() / ready tasks / topological ties use compareIds
// (numeric ascending, then strings lexicographic). Neighbour lists keep insertion order.
//
// Complexity (V vertices, E edges, d = degree of the vertices involved):
//   addVertex            O(1) average         removeVertex   O(d) (+ O(d) per neighbour list)
//   addEdge / removeEdge O(d) (duplicate scan) hasEdge        O(d)
//   getPrerequisites / getDependents  O(d)    wouldCreateCycle O(V + E)
//   hasCycle / findCycle O(V + E) iterative DFS (+ O(V log V) deterministic root order)
//   topologicalSort      O((V + E) log V) Kahn with a heap of eligible vertices
//   readyTasks           O(V + E + C + R log R)  (C completed ids, R results)
//   cyclicComponents / cycleAffectedVertices / readyTasksExcludingCycles   O(V + E) (+ sorting)
// Space O(V + E). No Crow / SQLite dependency.
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "models/Validation.hpp"

namespace chronoplan::algorithms {

class GraphError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class SelfEdgeError : public GraphError { public: using GraphError::GraphError; };
class DuplicateEdgeError : public GraphError { public: using GraphError::GraphError; };
class MissingVertexError : public GraphError { public: using GraphError::GraphError; };
class CycleError : public GraphError { public: using GraphError::GraphError; };

struct TopologicalSortResult {
    bool success = false;
    std::vector<models::Id> order;        // complete order when success, else the sortable prefix
    std::vector<models::Id> unresolved;   // on failure: vertices on or downstream of a cycle
};

class DependencyGraph {
public:
    // False if the vertex already exists. GraphError for an empty id.
    bool addVertex(const models::Id& id);
    // Removes the vertex and every incident edge. False if absent.
    bool removeVertex(const models::Id& id);
    bool hasVertex(const models::Id& id) const;

    // prerequisite -> dependent. Throws SelfEdgeError, MissingVertexError, DuplicateEdgeError.
    // Cycles are NOT rejected here (use wouldCreateCycle / hasCycle).
    void addEdge(const models::Id& prerequisite, const models::Id& dependent);
    bool removeEdge(const models::Id& prerequisite, const models::Id& dependent);
    bool hasEdge(const models::Id& prerequisite, const models::Id& dependent) const;

    // Direct neighbours; MissingVertexError if the vertex does not exist.
    std::vector<models::Id> getPrerequisites(const models::Id& id) const;
    std::vector<models::Id> getDependents(const models::Id& id) const;
    std::vector<models::Id> getAdjacent(const models::Id& id) const { return getDependents(id); }

    // True if adding prerequisite -> dependent would close a cycle (also for equal ids).
    bool wouldCreateCycle(const models::Id& prerequisite, const models::Id& dependent) const;
    bool hasCycle() const { return findCycle().has_value(); }
    // One cycle as v0 -> v1 -> ... -> vk (-> v0), or std::nullopt for a DAG.
    std::optional<std::vector<models::Id>> findCycle() const;

    // Kahn's algorithm; among equally eligible vertices the smallest id (compareIds) goes first.
    TopologicalSortResult topologicalSort() const;
    // Throws CycleError when no ordering exists.
    std::vector<models::Id> topologicalOrder() const;

    // Vertices not in `completed` whose prerequisites are ALL in `completed`
    // (isolated vertices qualify). Unknown completed ids are ignored. Sorted by compareIds.
    std::vector<models::Id> readyTasks(const std::vector<models::Id>& completed) const;

    // Phase 4: cycle-aware readiness. readyTasks() only looks at direct prerequisites, so in a
    // cycle A <-> B with A "completed", B looks ready. These helpers close that hole.
    //
    // Strongly connected components that contain a cycle (size > 1; self-edges cannot exist).
    // Every vertex on any dependency cycle is in exactly one component. Members sorted by
    // compareIds, components sorted by their first member. Iterative Tarjan, O(V + E).
    std::vector<std::vector<models::Id>> cyclicComponents() const;
    // Vertices reachable from `seeds` along prerequisite -> dependent edges, EXCLUDING the seeds.
    std::vector<models::Id> reachableFrom(const std::vector<models::Id>& seeds) const;
    // Every vertex on a cycle plus everything downstream of one (sorted). These can never be
    // scheduled: a cycle member can never become ready and a downstream task waits on it forever.
    std::vector<models::Id> cycleAffectedVertices() const;
    // readyTasks(completed) minus cycleAffectedVertices(): a task that belongs to a cycle (or
    // sits downstream of one) is never ready, even if all its direct prerequisites are completed.
    std::vector<models::Id> readyTasksExcludingCycles(const std::vector<models::Id>& completed) const;

    std::vector<models::Id> vertices() const;   // sorted by compareIds
    size_t vertexCount() const noexcept { return adj_.size(); }
    size_t edgeCount() const noexcept { return edges_; }
    bool empty() const noexcept { return adj_.empty(); }
    void clear() noexcept;

private:
    struct Vertex {
        models::Id id;
        std::vector<std::string> out;   // keys of dependents
        std::vector<std::string> in;    // keys of prerequisites
    };
    const Vertex& require(const models::Id& id) const;
    std::vector<models::Id> idsOf(const std::vector<std::string>& keys) const;

    std::unordered_map<std::string, Vertex> adj_;
    size_t edges_ = 0;
};

}  // namespace chronoplan::algorithms
