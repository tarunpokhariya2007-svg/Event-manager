#pragma once
// Scheduling objectives = the ranking rule of the eligible-task PriorityQueue.
// Every objective only changes WHICH ready task is placed next; hard constraints (events,
// availability, dependencies, deadlines, windows) are enforced by the placer for all of them.
//
//   priority  Phase 3 rule (compareCandidates): higher priority, then earlier deadline, ...
//   deadline  earliest deadline DATE first; within the same date higher priority wins.
//   edf       pure earliest-deadline-first on the exact deadline minute (priority ignored).
//   balanced  score = priority*100 + urgency (0..300, grows as the deadline gets closer).
//   compact   shortest duration first (packs many small tasks, fewer fragments for big ones).
// Tasks without a deadline rank after tasks with one in `deadline`/`edf`. Every rule ends in the
// task id, so the order is total and deterministic.
#include <string>
#include "algorithms/PriorityQueue.hpp"
#include "algorithms/SchedulingPriority.hpp"

namespace chronoplan::scheduler {

enum class Objective { Balanced, Priority, Deadline, Compact, Edf };

const char* toString(Objective o);
Objective parseObjective(const std::string& name);   // throws models::ValidationError

// Urgency points 0..300: 300 for a deadline today or earlier, minus 30 per whole day away.
int urgencyPoints(const algorithms::SchedulingCandidate& c);
int balancedScore(const algorithms::SchedulingCandidate& c);

// <0: a is placed before b. Total order (ends with compareIds).
int compareForObjective(Objective o, const algorithms::SchedulingCandidate& a,
                        const algorithms::SchedulingCandidate& b);

struct ObjectiveRanksBefore {
    Objective objective = Objective::Balanced;
    bool operator()(const algorithms::SchedulingCandidate& a, const algorithms::SchedulingCandidate& b) const {
        return compareForObjective(objective, a, b) < 0;
    }
};

// The Phase 3 PriorityQueue, instantiated with the objective-aware comparator.
using ObjectiveQueue = algorithms::PriorityQueue<algorithms::SchedulingCandidate, ObjectiveRanksBefore,
                                                 algorithms::CandidateKey>;

}  // namespace chronoplan::scheduler
