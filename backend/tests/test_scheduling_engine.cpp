// Phase 4 engine tests: basic scheduling, fixed events, objectives, dependencies, deadlines,
// splitting, multi-day horizons, determinism and a randomised property test. No database.
#include <random>
#include "SchedTestUtil.hpp"
#include "scheduler/SchedulingObjective.hpp"

using namespace schedtest;
using nlohmann::json;

// ===================================================================== A. basic scheduling

TEST(EngineBasic, OneTaskInEmptyDay) {
    const auto r = run(body(cfg(), {task("a", 60)}));
    EXPECT_VALID(r);
    EXPECT_EQ(r.status, models::ResultStatus::Success);
    const auto s = sessionsOf(r, "a");
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0].start, H(9));
    EXPECT_EQ(s[0].end, H(10));
    EXPECT_EQ(s[0].date, kMon);
    EXPECT_EQ(r.total_allocated_min, 60);
    EXPECT_EQ(r.total_available_min, 480);
    EXPECT_TRUE(r.unscheduled_tasks.empty());
    EXPECT_EQ(r.objective, "priority");
    EXPECT_EQ(r.start_date, kMon);
    EXPECT_EQ(r.end_date, kMon);
}

TEST(EngineBasic, MultipleTasksFillTheDayInPriorityOrder) {
    const auto r = run(body(cfg(), {task("a", 60, {{"priority", 2}}), task("b", 60, {{"priority", 5}}),
                                    task("c", 60, {{"priority", 3}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(taskOrder(r), (std::vector<std::string>{"b", "c", "a"}));
    EXPECT_EQ(completionOf(r, "a"), H(12));
    EXPECT_EQ(r.status, models::ResultStatus::Success);
}

TEST(EngineBasic, TwoAvailabilityWindowsNeverUseTheBreak) {
    const json c = cfg({{"availability_windows", json::array({{{"start_min", H(9)}, {"end_min", H(12)}},
                                                               {{"start_min", H(13)}, {"end_min", H(18)}}})}});
    // 300 minutes, splittable, min chunk 60: 9-12 (180) then 13-15 (120). 12:00-13:00 stays empty.
    const auto r = run(body(c, {task("a", 300, {{"allow_split", true}, {"min_chunk_min", 60}})}));
    EXPECT_VALID(r);
    const auto s = sessionsOf(r, "a");
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].start, H(9));  EXPECT_EQ(s[0].end, H(12));
    EXPECT_EQ(s[1].start, H(13)); EXPECT_EQ(s[1].end, H(15));
    for (const auto& x : s) EXPECT_TRUE(x.end <= H(12) || x.start >= H(13));
    EXPECT_EQ(r.total_available_min, 480);   // 180 + 300
}

TEST(EngineBasic, NonSplittableTaskNeedsOneWindow) {
    const json c = cfg({{"availability_windows", json::array({{{"start_min", H(9)}, {"end_min", H(12)}},
                                                               {{"start_min", H(13)}, {"end_min", H(18)}}})}});
    const auto r = run(body(c, {task("a", 240)}));   // does not fit 9-12, fits 13-18
    EXPECT_VALID(r);
    const auto s = sessionsOf(r, "a");
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0].start, H(13));
    EXPECT_EQ(s[0].end, H(17));
}

TEST(EngineBasic, MultiDayHorizonProgressesDates) {
    const auto r = run(body(cfg({{"end_date", "2026-11-04"}}), {task("a", 480), task("b", 480), task("c", 480)}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].date, "2026-11-02");
    EXPECT_EQ(sessionsOf(r, "b")[0].date, "2026-11-03");
    EXPECT_EQ(sessionsOf(r, "c")[0].date, "2026-11-04");
    EXPECT_EQ(r.end_date, "2026-11-04");
    EXPECT_EQ(r.total_available_min, 3 * 480);
}

TEST(EngineBasic, EmptyTaskListAndEmptyEvents) {
    const auto r = run(body(cfg()));
    EXPECT_VALID(r);
    EXPECT_EQ(r.status, models::ResultStatus::Success);
    EXPECT_TRUE(r.timeline.empty());
    EXPECT_EQ(r.statistics.tasks_considered, 0);
    EXPECT_EQ(r.total_available_min, 480);
    ASSERT_EQ(r.available_slots.size(), 1u);
    EXPECT_EQ(r.available_slots[0].start_min, H(9));
    EXPECT_EQ(r.available_slots[0].end_min, H(17));
    EXPECT_EQ(r.statistics.utilization_pct, 0.0);
}

TEST(EngineBasic, EmptyEventsListWithTasksAndEmptyTasksWithEvents) {
    const auto onlyEvents = run(body(cfg(), json::array(), {event("e", H(10), H(11))}));
    EXPECT_VALID(onlyEvents);
    ASSERT_EQ(onlyEvents.timeline.size(), 1u);
    EXPECT_EQ(onlyEvents.timeline[0].status, models::TimelineStatus::Fixed);
    EXPECT_EQ(onlyEvents.total_available_min, 420);
}

TEST(EngineBasic, NoAvailableTimeMakesEverythingUnscheduled) {
    // Only Saturdays are working days; the horizon is a Monday.
    const auto r = run(body(cfg({{"working_weekdays", json::array({"sat"})}}), {task("a", 60)}));
    EXPECT_VALID(r);
    EXPECT_EQ(r.status, models::ResultStatus::Infeasible);
    const auto* u = unscheduled(r, "a");
    ASSERT_NE(u, nullptr);
    EXPECT_FALSE(u->reason.empty());
    EXPECT_EQ(u->remaining_min, 60);
    EXPECT_EQ(r.total_available_min, 0);
    EXPECT_TRUE(r.available_slots.empty());
}

TEST(EngineBasic, CompletedAndCancelledTasksAreNeverPlaced) {
    const auto r = run(body(cfg(), {task("a", 60, {{"status", "completed"}}), task("b", 60, {{"status", "Cancelled"}}),
                                    task("c", 60, {{"status", " DONE "}}), task("d", 60, {{"status", "in progress"}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a").size(), 0u);
    EXPECT_EQ(sessionsOf(r, "b").size(), 0u);
    EXPECT_EQ(sessionsOf(r, "c").size(), 0u);
    EXPECT_EQ(sessionsOf(r, "d").size(), 1u);
    EXPECT_EQ(r.statistics.tasks_skipped_terminal, 3);
    EXPECT_EQ(r.statistics.tasks_considered, 1);
    EXPECT_EQ(r.status, models::ResultStatus::Success);
    EXPECT_TRUE(r.unscheduled_tasks.empty());   // terminal tasks are not "unscheduled"
}

TEST(EngineBasic, AllTasksCompletedIsASuccessfulEmptySchedule) {
    const auto r = run(body(cfg(), {task("a", 60, {{"status", "completed"}}), task("b", 60, {{"status", "canceled"}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(r.status, models::ResultStatus::Success);
    EXPECT_TRUE(r.timeline.empty());
}

TEST(EngineBasic, TasksWithoutIdsGetNumericIds) {
    json t = {{"title", "no id"}, {"duration_min", 30}};
    json t2 = {{"id", 5}, {"title", "five"}, {"duration_min", 30}};
    const auto r = run(body(cfg(), json::array({t, t2})));
    EXPECT_VALID(r);
    ASSERT_EQ(r.timeline.size(), 2u);
    std::set<std::string> ids;
    for (const auto& e : r.timeline) { ids.insert(e.reference_id.value); EXPECT_TRUE(e.reference_id.numeric); }
    EXPECT_EQ(ids, (std::set<std::string>{"5", "6"}));
}

TEST(EngineBasic, TimelineIdsAndOrderAreChronological) {
    const auto r = run(body(cfg({{"end_date", "2026-11-03"}}), {task("a", 480), task("b", 60)},
                            {event("e1", H(15), H(16)), event("e2", H(8), H(9), "2026-11-03")}));
    EXPECT_VALID(r);
    int n = 0;
    std::string prevDate; int prevStart = -1;
    for (const auto& e : r.timeline) {
        EXPECT_EQ(e.id, "tl-" + std::to_string(++n));
        if (e.date == prevDate) EXPECT_GE(e.start_min, prevStart);
        else EXPECT_GT(e.date, prevDate);
        prevDate = e.date; prevStart = e.start_min;
    }
}

// ===================================================================== B. fixed events

TEST(EngineEvents, TaskGoesAfterAnEventThatBlocksTheStart) {
    const auto r = run(body(cfg(), {task("a", 120), task("b", 60, {{"priority", 1}})}, {event("e", H(10), H(11))}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(11));   // 9-10 is too short for 120 minutes
    EXPECT_EQ(sessionsOf(r, "b")[0].start, H(9));    // the gap is used by the other task
}

TEST(EngineEvents, MultipleEventsLeaveOnlyUsableGaps) {
    const auto r = run(body(cfg(), {task("a", 60), task("b", 90, {{"priority", 1}})},
                            {event("e1", H(9), H(10)), event("e2", H(11), H(12))}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(10));
    EXPECT_EQ(sessionsOf(r, "b")[0].start, H(12));
    EXPECT_EQ(sessionsOf(r, "b")[0].end, H(13, 30));
}

TEST(EngineEvents, EventAtStartAndEndOfDay) {
    const auto r = run(body(cfg(), {task("a", 420)}, {event("e1", H(9), H(10)), event("e2", H(16), H(17))}));
    EXPECT_VALID(r);   // 10:00-16:00 is only 360: 420 minutes do not fit
    EXPECT_NE(unscheduled(r, "a"), nullptr);
    const auto r2 = run(body(cfg(), {task("a", 360)}, {event("e1", H(9), H(10)), event("e2", H(16), H(17))}));
    EXPECT_VALID(r2);
    EXPECT_EQ(sessionsOf(r2, "a")[0].start, H(10));
    EXPECT_EQ(sessionsOf(r2, "a")[0].end, H(16));
}

TEST(EngineEvents, TouchingEventsAreNotConflictsButOverlappingOnesAre) {
    const auto touching = run(body(cfg(), json::array(), {event("e1", H(10), H(11)), event("e2", H(11), H(12))}));
    EXPECT_TRUE(touching.conflicts.empty());
    const auto overlapping = run(body(cfg(), json::array(), {event("e1", H(10), H(11)), event("e2", H(10, 30), H(11, 30))}));
    ASSERT_EQ(overlapping.conflicts.size(), 1u);
    const auto& cf = overlapping.conflicts[0];
    EXPECT_EQ(cf.id, "cf-1");
    EXPECT_EQ(cf.date, kMon);
    EXPECT_EQ(cf.overlap_start_min, H(10, 30));
    EXPECT_EQ(cf.overlap_end_min, H(11));
    EXPECT_EQ(cf.event_a_id.value, "e1");
    EXPECT_EQ(cf.event_b_id.value, "e2");
    EXPECT_EQ(overlapping.timeline.size(), 2u);   // both events stay, unmoved
}

TEST(EngineEvents, EventOutsideTheHorizonHasNoEffect) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("e", H(9), H(12), "2026-11-03"), event("e2", H(9), H(12), "2026-10-31")}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(9));
    EXPECT_EQ(r.statistics.fixed_events, 0);
    for (const auto& e : r.timeline) EXPECT_EQ(e.type, models::TimelineType::Task);
    ASSERT_FALSE(r.warnings.empty());
    EXPECT_NE(r.warnings[0].find("outside the scheduling horizon"), std::string::npos);
}

TEST(EngineEvents, OverlappingFixedEventsAreReportedAndTasksAvoidBoth) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("e1", H(9), H(10)), event("e2", H(9, 30), H(10, 30))}));
    EXPECT_VALID(r);
    EXPECT_EQ(r.conflicts.size(), 1u);
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(10, 30));   // after the union of both events
}

// ---- overall status with overlapping events (conflicts) ----
TEST(EngineStatus, NoConflictsAndAllTasksScheduledIsSuccess) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("e1", H(9), H(10)), event("e2", H(11), H(12))}));
    EXPECT_VALID(r);
    EXPECT_TRUE(r.conflicts.empty());
    EXPECT_TRUE(r.unscheduled_tasks.empty());
    EXPECT_EQ(r.status, models::ResultStatus::Success);
}

TEST(EngineStatus, OverlappingEventsWithAllTasksScheduledIsNotSuccess) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("e1", H(9), H(10)), event("e2", H(9, 30), H(10, 30))}));
    EXPECT_VALID(r);
    ASSERT_EQ(r.conflicts.size(), 1u);
    EXPECT_TRUE(r.unscheduled_tasks.empty());
    EXPECT_EQ(sessionsOf(r, "a").size(), 1u);            // task scheduling itself is unchanged
    EXPECT_EQ(r.status, models::ResultStatus::Partial);
}

TEST(EngineStatus, OverlappingEventsWithPartiallyScheduledTasksIsPartial) {
    // 09:00-10:00 and 09:30-10:30 are blocked, so 10:30-17:00 (390 min) is free: "a" fits, "b" does not.
    const auto r = run(body(cfg(), {task("a", 300), task("b", 300, {{"priority", 1}})},
                            {event("e1", H(9), H(10)), event("e2", H(9, 30), H(10, 30))}));
    EXPECT_VALID(r);
    ASSERT_EQ(r.conflicts.size(), 1u);
    EXPECT_FALSE(sessionsOf(r, "a").empty());
    EXPECT_NE(unscheduled(r, "b"), nullptr);
    EXPECT_EQ(r.status, models::ResultStatus::Partial);
}

TEST(EngineStatus, TouchingEventsDoNotOverlapAndStaySuccess) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("e1", H(10), H(11)), event("e2", H(11), H(12))}));
    EXPECT_VALID(r);
    EXPECT_TRUE(r.conflicts.empty());
    EXPECT_EQ(r.status, models::ResultStatus::Success);
}

TEST(EngineEvents, FlexibleEventsKeepTheirStoredIntervals) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("f", H(9), H(10), kMon, {{"is_flexible", true}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(10));
    for (const auto& e : r.timeline)
        if (e.type == models::TimelineType::Event) {
            EXPECT_EQ(e.start_min, H(9));
            EXPECT_EQ(e.end_min, H(10));
            EXPECT_NE(e.explanation.find("never moves"), std::string::npos);
        }
}

TEST(EngineEvents, FixedEventsAreNeverCountedAsScheduledTaskTime) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("e", H(9), H(11))}));
    EXPECT_EQ(r.statistics.total_scheduled_min, 60);
    EXPECT_EQ(r.statistics.fixed_event_window_min, 120);
    EXPECT_EQ(r.statistics.total_available_min, 360);
    EXPECT_EQ(r.statistics.total_occupied_min, 180);
    EXPECT_DOUBLE_EQ(r.statistics.utilization_pct, 16.67);   // 60 / 360
}

TEST(EngineEvents, InvalidEventIsRejected) {
    json bad = event("e", H(10), H(9));
    EXPECT_THROW(models::SchedulingRequest::fromJson(body(cfg(), json::array(), {bad})), models::ValidationError);
}

// ===================================================================== C. priority / objectives

TEST(EngineObjectives, PriorityFirstPlacesCriticalBeforeLow) {
    const auto r = run(body(cfg({{"objective", "priority"}}), {task("low", 60, {{"priority", 1}}), task("crit", 60, {{"priority", 5}})}));
    EXPECT_EQ(taskOrder(r), (std::vector<std::string>{"crit", "low"}));
}

TEST(EngineObjectives, DeadlineFirstPlacesEarlierDeadlineDatesFirst) {
    const json tasks = json::array({task("x", 60, {{"priority", 5}}),
                                    task("y", 60, {{"priority", 1}, {"deadline_date", "2026-11-03"}}),
                                    task("z", 60, {{"priority", 3}, {"deadline_date", kMon}})});
    const auto r = run(body(cfg({{"objective", "deadline"}, {"end_date", "2026-11-03"}}), tasks));
    EXPECT_VALID(r);
    EXPECT_EQ(taskOrder(r), (std::vector<std::string>{"z", "y", "x"}));
}

TEST(EngineObjectives, DeadlineObjectiveUsesPriorityWithinTheSameDateButEdfDoesNot) {
    const json tasks = json::array({task("p", 60, {{"priority", 1}, {"deadline_date", kMon}, {"deadline_min", H(12)}}),
                                    task("q", 60, {{"priority", 5}, {"deadline_date", kMon}, {"deadline_min", H(17)}})});
    EXPECT_EQ(taskOrder(run(body(cfg({{"objective", "deadline"}}), tasks))), (std::vector<std::string>{"q", "p"}));
    EXPECT_EQ(taskOrder(run(body(cfg({{"objective", "edf"}}), tasks))), (std::vector<std::string>{"p", "q"}));
}

TEST(EngineObjectives, BalancedWeighsImportanceAndUrgency) {
    // balanced: score = priority*100 + urgency. b = 500; a (prio 3, due today) = 300 + 300 = 600.
    const json tasks = json::array({task("b", 60, {{"priority", 5}}),
                                    task("a", 60, {{"priority", 3}, {"deadline_date", kMon}})});
    EXPECT_EQ(taskOrder(run(body(cfg({{"objective", "balanced"}}), tasks))), (std::vector<std::string>{"a", "b"}));
    // The priority objective ignores urgency: the higher priority wins.
    EXPECT_EQ(taskOrder(run(body(cfg({{"objective", "priority"}}), tasks))), (std::vector<std::string>{"b", "a"}));
}

TEST(EngineObjectives, BalancedDiffersFromPriorityWhenUrgencyDominates) {
    // d: prio 4 due tomorrow -> 400 + 270 = 670 ; b: prio 5, no deadline -> 500.
    const json tasks = json::array({task("b", 60, {{"priority", 5}}),
                                    task("d", 60, {{"priority", 4}, {"deadline_date", "2026-11-03"}})});
    const json c = {{"end_date", "2026-11-03"}};
    json bal = c; bal["objective"] = "balanced";
    json pri = c; pri["objective"] = "priority";
    EXPECT_EQ(taskOrder(run(body(cfg(bal), tasks))), (std::vector<std::string>{"d", "b"}));
    EXPECT_EQ(taskOrder(run(body(cfg(pri), tasks))), (std::vector<std::string>{"b", "d"}));
}

TEST(EngineObjectives, CompactPlacesShortTasksFirst) {
    const json tasks = json::array({task("long", 120, {{"priority", 5}}), task("short", 30, {{"priority", 1}})});
    EXPECT_EQ(taskOrder(run(body(cfg({{"objective", "compact"}}), tasks))), (std::vector<std::string>{"short", "long"}));
    EXPECT_EQ(taskOrder(run(body(cfg({{"objective", "priority"}}), tasks))), (std::vector<std::string>{"long", "short"}));
}

TEST(EngineObjectives, EqualPriorityAndDeadlineAreBrokenByDurationThenId) {
    const json tasks = json::array({task(3, 60), task(1, 60), task(2, 60), task(9, 90)});
    for (const char* obj : {"priority", "deadline", "balanced", "edf"}) {
        const auto r = run(body(cfg({{"objective", obj}}), tasks));
        const auto order = taskOrder(r);
        ASSERT_EQ(order.size(), 4u) << obj;
        // edf ignores duration, so only check the id tie-break among the equal 60-minute tasks.
        std::vector<std::string> sixty;
        for (const auto& id : order) if (id != "9") sixty.push_back(id);
        EXPECT_EQ(sixty, (std::vector<std::string>{"1", "2", "3"})) << obj;
    }
    EXPECT_EQ(taskOrder(run(body(cfg({{"objective", "priority"}}), tasks))).front(), "9");   // longest first
}

TEST(EngineObjectives, ListOrderOfTheInputDoesNotChangeTheSchedule) {
    json a = json::array({task("a", 60, {{"priority", 3}}), task("b", 60, {{"priority", 3}}), task("c", 45, {{"priority", 4}})});
    json b = json::array({a[2], a[1], a[0]});
    for (const char* obj : {"priority", "deadline", "balanced", "compact", "edf"}) {
        auto r1 = run(body(cfg({{"objective", obj}}), a)).toApiJson();
        auto r2 = run(body(cfg({{"objective", obj}}), b)).toApiJson();
        for (auto* r : {&r1, &r2}) (*r)["solver_duration_ms"] = 0;
        // explanations mention the rank only, which is identical; the timelines must match exactly
        EXPECT_EQ(r1["timeline"], r2["timeline"]) << obj;
    }
}

TEST(EngineObjectives, ObjectiveComparatorIsATotalOrderAndParsesNames) {
    using namespace scheduler;
    EXPECT_THROW(parseObjective("fastest"), models::ValidationError);
    for (const char* n : {"balanced", "priority", "deadline", "compact", "edf"}) EXPECT_STREQ(toString(parseObjective(n)), n);
    algorithms::SchedulingCandidate a, b;
    a.task_id.value = "1"; a.task_id.numeric = true; b = a; b.task_id.value = "2";
    for (Objective o : {Objective::Balanced, Objective::Priority, Objective::Deadline, Objective::Compact, Objective::Edf}) {
        EXPECT_LT(compareForObjective(o, a, b), 0);
        EXPECT_GT(compareForObjective(o, b, a), 0);
        EXPECT_EQ(compareForObjective(o, a, a), 0);
    }
}

TEST(EngineObjectives, EveryObjectivePreservesHardConstraints) {
    json tasks = json::array({task("a", 120, {{"priority", 5}, {"deadline_date", kMon}, {"deadline_min", H(12)}}),
                              task("b", 90, {{"priority", 2}, {"earliest_start_min", H(13)}}),
                              task("c", 60, {{"priority", 4}, {"prerequisite_ids", json::array({"b"})}})});
    for (const char* obj : {"priority", "deadline", "balanced", "compact", "edf"}) {
        const auto r = run(body(cfg({{"objective", obj}}), tasks, {event("e", H(10), H(11))}));
        EXPECT_VALID(r);
        EXPECT_GE(sessionsOf(r, "b")[0].start, H(13)) << obj;
        EXPECT_GE(sessionsOf(r, "c")[0].start, completionOf(r, "b")) << obj;
        EXPECT_LE(completionOf(r, "a"), H(12)) << obj;
    }
}

// ===================================================================== D. dependencies

TEST(EngineDependencies, SimpleChainRunsInDependencyOrderEvenAgainstPriority) {
    // C requires B requires A; C has the highest priority but cannot go first.
    const json tasks = json::array({task("A", 60, {{"priority", 1}}),
                                    task("B", 60, {{"priority", 3}, {"prerequisite_ids", json::array({"A"})}}),
                                    task("C", 60, {{"priority", 5}, {"prerequisite_ids", json::array({"B"})}})});
    const auto r = run(body(cfg(), tasks));
    EXPECT_VALID(r);
    EXPECT_EQ(taskOrder(r), (std::vector<std::string>{"A", "B", "C"}));
    EXPECT_GE(sessionsOf(r, "B")[0].start, completionOf(r, "A"));
    EXPECT_GE(sessionsOf(r, "C")[0].start, completionOf(r, "B"));
}

TEST(EngineDependencies, TaskWaitsForAllPrerequisitesNotJustOne) {
    //   A -> B -> D      A, B, C are prerequisites of D; C is long and low priority
    //   A -> C -> D
    const json tasks = json::array({
        task("A", 30, {{"priority", 5}}),
        task("B", 30, {{"priority", 4}, {"prerequisite_ids", json::array({"A"})}}),
        task("C", 180, {{"priority", 3}, {"prerequisite_ids", json::array({"A"})}}),
        task("D", 30, {{"priority", 5}, {"prerequisite_ids", json::array({"B", "C"})}})});
    const auto r = run(body(cfg(), tasks));
    EXPECT_VALID(r);
    const int dStart = sessionsOf(r, "D")[0].start;
    EXPECT_GE(dStart, completionOf(r, "B"));
    EXPECT_GE(dStart, completionOf(r, "C"));
    EXPECT_EQ(dStart, completionOf(r, "C"));   // C is the later one
}

TEST(EngineDependencies, CompletedPrerequisiteUnlocksTheDependent) {
    const json tasks = json::array({task("A", 60, {{"status", "completed"}}),
                                    task("B", 60, {{"prerequisite_ids", json::array({"A"})}})});
    const auto r = run(body(cfg(), tasks));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "B")[0].start, H(9));
    EXPECT_EQ(sessionsOf(r, "A").size(), 0u);
}

TEST(EngineDependencies, IncompletePrerequisiteBlocksTheDependent) {
    // A cannot be placed (needs 600 non-split minutes in an 8 h day), so B must stay blocked.
    const json tasks = json::array({task("A", 600), task("B", 30, {{"prerequisite_ids", json::array({"A"})}})});
    const auto r = run(body(cfg(), tasks));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "B").size(), 0u);
    const auto* u = unscheduled(r, "B");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->reason_code, "blocked_by_prerequisite");
    EXPECT_NE(u->reason.find("TA"), std::string::npos);   // names the prerequisite
}

TEST(EngineDependencies, BlockingPropagatesThroughTheChain) {
    const json tasks = json::array({task("A", 600), task("B", 30, {{"prerequisite_ids", json::array({"A"})}}),
                                    task("C", 30, {{"prerequisite_ids", json::array({"B"})}})});
    const auto r = run(body(cfg(), tasks));
    EXPECT_EQ(unscheduled(r, "B")->reason_code, "blocked_by_prerequisite");
    EXPECT_EQ(unscheduled(r, "C")->reason_code, "blocked_by_prerequisite");
}

TEST(EngineDependencies, CancelledPrerequisiteBlocks) {
    const json tasks = json::array({task("A", 60, {{"status", "cancelled"}}),
                                    task("B", 60, {{"prerequisite_ids", json::array({"A"})}})});
    const auto r = run(body(cfg(), tasks));
    EXPECT_EQ(unscheduled(r, "B")->reason_code, "prerequisite_cancelled");
}

TEST(EngineDependencies, DirectCycleIsNeverScheduledButIndependentTasksAre) {
    const json tasks = json::array({task("A", 60, {{"prerequisite_ids", json::array({"B"})}}),
                                    task("B", 60, {{"prerequisite_ids", json::array({"A"})}}), task("C", 60)});
    const auto r = run(body(cfg(), tasks));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "A").size() + sessionsOf(r, "B").size(), 0u);
    EXPECT_EQ(unscheduled(r, "A")->reason_code, "circular_dependency");
    EXPECT_EQ(unscheduled(r, "B")->reason_code, "circular_dependency");
    EXPECT_EQ(sessionsOf(r, "C").size(), 1u);
    EXPECT_EQ(r.status, models::ResultStatus::Partial);
}

TEST(EngineDependencies, IndirectCycleAndMultipleDisconnectedCycles) {
    const json tasks = json::array({
        task("A", 30, {{"prerequisite_ids", json::array({"C"})}}), task("B", 30, {{"prerequisite_ids", json::array({"A"})}}),
        task("C", 30, {{"prerequisite_ids", json::array({"B"})}}),                          // A->B->C->A
        task("X", 30, {{"prerequisite_ids", json::array({"Y"})}}), task("Y", 30, {{"prerequisite_ids", json::array({"X"})}}),   // X<->Y
        task("Z", 30)});
    const auto r = run(body(cfg(), tasks));
    EXPECT_VALID(r);
    for (const char* id : {"A", "B", "C", "X", "Y"}) {
        EXPECT_EQ(sessionsOf(r, id).size(), 0u) << id;
        EXPECT_EQ(unscheduled(r, id)->reason_code, "circular_dependency") << id;
    }
    EXPECT_EQ(sessionsOf(r, "Z").size(), 1u);
}

TEST(EngineDependencies, TaskDownstreamOfACycleStaysBlocked) {
    const json tasks = json::array({task("A", 30, {{"prerequisite_ids", json::array({"B"})}}),
                                    task("B", 30, {{"prerequisite_ids", json::array({"A"})}}),
                                    task("D", 30, {{"prerequisite_ids", json::array({"A"})}}),
                                    task("E", 30, {{"prerequisite_ids", json::array({"D"})}})});
    const auto r = run(body(cfg(), tasks));
    EXPECT_EQ(unscheduled(r, "D")->reason_code, "blocked_by_cycle");
    EXPECT_EQ(unscheduled(r, "E")->reason_code, "blocked_by_cycle");
    EXPECT_EQ(r.timeline.size(), 0u);
    EXPECT_EQ(r.status, models::ResultStatus::Infeasible);
}

TEST(EngineDependencies, CycleMemberIsNotScheduledJustBecauseItsPrerequisiteIsCompleted) {
    // The Phase 3 bug: A <-> B with A "completed" made B look ready (its only prerequisite is done).
    const json tasks = json::array({task("A", 30, {{"status", "completed"}, {"prerequisite_ids", json::array({"B"})}}),
                                    task("B", 30, {{"prerequisite_ids", json::array({"A"})}}),
                                    task("E", 30, {{"prerequisite_ids", json::array({"B"})}})});
    const auto r = run(body(cfg(), tasks));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "B").size(), 0u);
    EXPECT_EQ(sessionsOf(r, "E").size(), 0u);
    EXPECT_EQ(unscheduled(r, "B")->reason_code, "circular_dependency");
}

TEST(EngineDependencies, DependentWaitsUntilAPrerequisiteEndsAcrossDays) {
    const json tasks = json::array({task("A", 480), task("B", 60, {{"priority", 5}, {"prerequisite_ids", json::array({"A"})}})});
    const auto r = run(body(cfg({{"end_date", "2026-11-03"}}), tasks));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "A")[0].date, "2026-11-02");
    EXPECT_EQ(sessionsOf(r, "B")[0].date, "2026-11-03");
    EXPECT_EQ(sessionsOf(r, "B")[0].start, H(9));
}

// ===================================================================== E. deadlines and time constraints

TEST(EngineDeadlines, DeadlineSatisfiedAndNotAtRiskWhenThereIsSlack) {
    const auto r = run(body(cfg(), {task("a", 60, {{"deadline_date", kMon}, {"deadline_min", H(17)}})}));
    EXPECT_VALID(r);
    EXPECT_LE(completionOf(r, "a"), H(17));
    EXPECT_EQ(atRisk(r, "a"), nullptr);
    EXPECT_EQ(r.statistics.tasks_meeting_deadline, 1);
}

TEST(EngineDeadlines, TaskFinishingRightAtItsDeadlineIsFlaggedAtRisk) {
    const auto r = run(body(cfg(), {task("a", 60, {{"deadline_date", kMon}, {"deadline_min", H(10)}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(completionOf(r, "a"), H(10));
    const auto* risk = atRisk(r, "a");
    ASSERT_NE(risk, nullptr);
    EXPECT_EQ(risk->risk_level, models::RiskLevel::AtRisk);
    EXPECT_EQ(risk->category, "deadline_risk");
    EXPECT_EQ(risk->buffer_min, 0);
    EXPECT_EQ(*risk->deadline, "2026-11-02T10:00");
    EXPECT_EQ(*risk->estimated_completion, "2026-11-02T10:00");
    EXPECT_EQ(unscheduled(r, "a"), nullptr);   // at risk, but scheduled
}

TEST(EngineDeadlines, DeadlineMissedBecauseNoTimeBeforeIt) {
    const auto r = run(body(cfg(), {task("a", 60, {{"deadline_date", kMon}, {"deadline_min", H(9, 30)}})}));
    EXPECT_VALID(r);
    const auto* u = unscheduled(r, "a");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->reason_code, "insufficient_time_before_deadline");
    const auto* risk = atRisk(r, "a");
    ASSERT_NE(risk, nullptr);
    EXPECT_EQ(risk->category, "insufficient_capacity");
    EXPECT_EQ(risk->risk_level, models::RiskLevel::ImpossibleWithoutRescheduling);
    EXPECT_LT(risk->buffer_min, 0);
    EXPECT_EQ(risk->required_min, 60);
    EXPECT_FALSE(risk->estimated_completion.has_value());
}

TEST(EngineDeadlines, DeadlineAlreadyPassedIsHardInfeasibleNotACapacityIssue) {
    const auto r = run(body(cfg(), {task("a", 60, {{"deadline_date", "2026-11-01"}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(unscheduled(r, "a")->reason_code, "deadline_exceeded");
    const auto* risk = atRisk(r, "a");
    ASSERT_NE(risk, nullptr);
    EXPECT_EQ(risk->category, "hard_infeasible");
    EXPECT_EQ(risk->risk_level, models::RiskLevel::ImpossibleWithoutRescheduling);
}

TEST(EngineDeadlines, InsufficientRemainingTimeSchedulesWhatFitsAndReportsTheRest) {
    // 300 splittable minutes, deadline 12:00 -> only 09:00-12:00 (180) fits.
    const auto r = run(body(cfg(), {task("a", 300, {{"allow_split", true}, {"min_chunk_min", 30},
                                                     {"deadline_date", kMon}, {"deadline_min", H(12)}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(scheduledMinutes(r, "a"), 180);
    const auto* u = unscheduled(r, "a");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->remaining_min, 120);
    EXPECT_EQ(u->reason_code, "insufficient_time_before_deadline");
    EXPECT_EQ(r.statistics.tasks_partially_scheduled, 1);
    EXPECT_EQ(r.status, models::ResultStatus::Partial);
    EXPECT_EQ(atRisk(r, "a")->category, "insufficient_capacity");
}

TEST(EngineDeadlines, NonSplittableTaskThatCannotFitBeforeItsDeadline) {
    // Free before 12:00: 09:00-10:00 and 11:00-12:00 only (event 10-11); 90 contiguous minutes needed.
    const auto r = run(body(cfg(), {task("a", 90, {{"deadline_date", kMon}, {"deadline_min", H(12)}})}, {event("e", H(10), H(11))}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a").size(), 0u);
    EXPECT_EQ(unscheduled(r, "a")->reason_code, "non_splittable_no_slot");
}

TEST(EngineDeadlines, LatestCompletionConstraint) {
    const auto fits = run(body(cfg(), {task("a", 120, {{"latest_completion_min", H(12)}})}));
    EXPECT_VALID(fits);
    EXPECT_LE(completionOf(fits, "a"), H(12));
    const auto tooShort = run(body(cfg(), {task("a", 120, {{"latest_completion_min", H(10)}})}));
    EXPECT_VALID(tooShort);
    EXPECT_EQ(unscheduled(tooShort, "a")->reason_code, "no_feasible_window");
}

TEST(EngineDeadlines, EarliestStartConstraintAppliesEveryDay) {
    const auto r = run(body(cfg({{"end_date", "2026-11-03"}}), {task("a", 60, {{"earliest_start_min", H(13)}}),
                                                                task("b", 420, {{"earliest_start_min", H(13)}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(13));
    // 420 minutes cannot fit 13:00-17:00 (240) in one block -> next day is no better -> unscheduled
    EXPECT_EQ(unscheduled(r, "b")->reason_code, "no_feasible_window");
}

TEST(EngineDeadlines, DateOnlyDeadlineMeansEndOfThatDay) {
    // deadline_date without deadline_min -> 24:00 of that date (policy documented in PHASE4_DESIGN.md).
    json t = task("a", 60, {{"deadline_date", "2026-11-03"}});
    const auto r = run(body(cfg({{"end_date", "2026-11-04"}}), {t}, {event("e", H(9), H(17))}));   // day 1 full -> day 2
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].date, "2026-11-03");
    EXPECT_EQ(r.statistics.tasks_meeting_deadline, 1);
    json t2 = task("b", 60, {{"deadline_date", kMon}});
    const auto r2 = run(body(cfg({{"end_date", "2026-11-03"}}), {t2}, {event("e", H(9), H(17))}));   // deadline day is full
    EXPECT_EQ(unscheduled(r2, "b")->reason_code, "insufficient_time_before_deadline");
    EXPECT_EQ(*atRisk(r2, "b")->deadline, "2026-11-02T24:00");
}

TEST(EngineDeadlines, DeadlineBeyondTheHorizonIsNotARisk) {
    const auto r = run(body(cfg(), {task("a", 60, {{"deadline_date", "2026-12-24"}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(atRisk(r, "a"), nullptr);
    EXPECT_EQ(r.statistics.tasks_meeting_deadline, 1);
}

TEST(EngineDeadlines, BlockedTaskWithADeadlineIsADependencyRiskNotACapacityRisk) {
    const json tasks = json::array({task("A", 600),
                                    task("B", 30, {{"prerequisite_ids", json::array({"A"})}, {"deadline_date", kMon}})});
    const auto r = run(body(cfg(), tasks));
    const auto* risk = atRisk(r, "B");
    ASSERT_NE(risk, nullptr);
    EXPECT_EQ(risk->category, "dependency_blocked");
    EXPECT_EQ(risk->risk_level, models::RiskLevel::AtRisk);
    // A has no deadline: unscheduled but NOT "at risk".
    EXPECT_EQ(atRisk(r, "A"), nullptr);
    EXPECT_NE(unscheduled(r, "A"), nullptr);
}

// ===================================================================== F. task splitting

TEST(EngineSplitting, SplittableTaskUsesSeveralGapsAroundEvents) {
    // Free: 09-10 (60), 11-12 (60), 13-17 (240). 180 minutes, min chunk 45 -> 60 + 60 + 60.
    const auto r = run(body(cfg(), {task("a", 180, {{"allow_split", true}, {"min_chunk_min", 45}})},
                            {event("e1", H(10), H(11)), event("e2", H(12), H(13))}));
    EXPECT_VALID(r);
    const auto s = sessionsOf(r, "a");
    ASSERT_EQ(s.size(), 3u);
    EXPECT_EQ(s[0].start, H(9));  EXPECT_EQ(s[0].end, H(10));
    EXPECT_EQ(s[1].start, H(11)); EXPECT_EQ(s[1].end, H(12));
    EXPECT_EQ(s[2].start, H(13)); EXPECT_EQ(s[2].end, H(14));
    EXPECT_EQ(scheduledMinutes(r, "a"), 180);
    int idx = 0;
    for (const auto& e : r.timeline)
        if (e.type == models::TimelineType::Task) { EXPECT_EQ(e.session_index, ++idx); EXPECT_EQ(e.session_count, 3); }
    EXPECT_EQ(r.statistics.session_count, 3);
    EXPECT_EQ(r.statistics.tasks_scheduled, 1);   // one task, not three
}

TEST(EngineSplitting, NonSplittableTaskStaysOneBlockAndAvoidsEvents) {
    const auto r = run(body(cfg(), {task("a", 180, {{"allow_split", false}})}, {event("e1", H(10), H(11)), event("e2", H(12), H(13))}));
    EXPECT_VALID(r);
    const auto s = sessionsOf(r, "a");
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0].start, H(13));
    EXPECT_EQ(s[0].end, H(16));
}

TEST(EngineSplitting, ConfigAllowSplittingFalseOverridesTheTask) {
    const auto r = run(body(cfg({{"allow_splitting", false}}), {task("a", 120, {{"allow_split", true}})}, {event("e", H(10), H(11))}));
    EXPECT_VALID(r);
    ASSERT_EQ(sessionsOf(r, "a").size(), 1u);
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(11));
}

TEST(EngineSplitting, MinimumChunkNeverViolatedAndNoShortFinalSession) {
    // Free slots: 60 minutes (09:00-10:00) and 50 (11:00-11:50). 100 minutes, min chunk 45.
    // Taking 60 first would leave 40 (< 45), so the engine takes 55 and leaves exactly 45.
    const json c = cfg({{"availability_windows", json::array({{{"start_min", H(9)}, {"end_min", H(10)}},
                                                              {{"start_min", H(11)}, {"end_min", H(11, 50)}}})}});
    const auto r = run(body(c, {task("a", 100, {{"allow_split", true}, {"min_chunk_min", 45}})}));
    EXPECT_VALID(r);
    const auto s = sessionsOf(r, "a");
    ASSERT_EQ(s.size(), 2u);
    for (const auto& x : s) EXPECT_GE(x.end - x.start, 45);
    EXPECT_EQ(scheduledMinutes(r, "a"), 100);
    EXPECT_EQ(s[0].end - s[0].start, 55);
    EXPECT_EQ(s[1].end - s[1].start, 45);
}

TEST(EngineSplitting, InsufficientChunkSpaceIsReportedNotPaddedWithShortSessions) {
    // Free slots 60 and 70, min chunk 60, 100 minutes: any split leaves a piece < 60 -> nothing placed.
    const json c = cfg({{"availability_windows", json::array({{{"start_min", H(9)}, {"end_min", H(10)}},
                                                              {{"start_min", H(11)}, {"end_min", H(12, 10)}}})}});
    const auto r = run(body(c, {task("a", 100, {{"allow_split", true}, {"min_chunk_min", 60}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a").size(), 0u);
    const auto* u = unscheduled(r, "a");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->reason_code, "min_chunk_unsatisfiable");
    EXPECT_EQ(u->remaining_min, 100);
}

TEST(EngineSplitting, PartialPlacementReportsRemainingWork) {
    // 200 minutes, min chunk 60; only 09:00-11:30 free (150): 150 would leave 50 < 60 -> 140? take
    // chunk = min(200,150)=150, left 50 <60 -> chunk = 140 (>=60), left 60. Then nothing else fits.
    const json c = cfg({{"availability_windows", json::array({{{"start_min", H(9)}, {"end_min", H(11, 30)}}})}});
    const auto r = run(body(c, {task("a", 200, {{"allow_split", true}, {"min_chunk_min", 60}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(scheduledMinutes(r, "a"), 140);
    const auto* u = unscheduled(r, "a");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->remaining_min, 60);
    EXPECT_EQ(r.statistics.tasks_partially_scheduled, 1);
    EXPECT_EQ(r.statistics.total_remaining_min, 60);
}

TEST(EngineSplitting, TaskSplitAcrossTwoDays) {
    // Day 1 has only 120 free minutes (event 09:00-12:00 within a 09:00-17:00... use window 09-12 and event 09-10).
    const json c = cfg({{"end_date", "2026-11-03"}, {"work_window_end_min", H(12)}});
    const auto r = run(body(c, {task("a", 300, {{"allow_split", true}, {"min_chunk_min", 30}})}, {event("e", H(9), H(10))}));
    EXPECT_VALID(r);
    const auto s = sessionsOf(r, "a");
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].date, "2026-11-02"); EXPECT_EQ(s[0].end - s[0].start, 120);
    EXPECT_EQ(s[1].date, "2026-11-03"); EXPECT_EQ(s[1].end - s[1].start, 180);
    EXPECT_EQ(scheduledMinutes(r, "a"), 300);
}

TEST(EngineSplitting, DependentWaitsForTheFinalSessionOfASplitPrerequisite) {
    const json tasks = json::array({task("A", 120, {{"allow_split", true}, {"min_chunk_min", 30}, {"priority", 1}}),
                                    task("B", 30, {{"priority", 5}, {"prerequisite_ids", json::array({"A"})}})});
    const auto r = run(body(cfg(), tasks, {event("e", H(10), H(11))}));   // A: 09-10 and 11-12
    EXPECT_VALID(r);
    ASSERT_EQ(sessionsOf(r, "A").size(), 2u);
    EXPECT_EQ(completionOf(r, "A"), H(12));
    EXPECT_GE(sessionsOf(r, "B")[0].start, H(12));
}

TEST(EngineSplitting, SessionsNeverOverlapEachOtherOrEvents) {
    const json tasks = json::array({task("a", 200, {{"allow_split", true}}), task("b", 150, {{"allow_split", true}}),
                                    task("c", 90, {{"allow_split", true}})});
    const auto r = run(body(cfg({{"end_date", "2026-11-03"}}), tasks, {event("e", H(11), H(12)), event("f", H(14), H(15), "2026-11-03")}));
    EXPECT_VALID(r);
    std::vector<std::pair<int, int>> day1;
    for (const auto& e : r.timeline) if (e.date == kMon) day1.push_back({e.start_min, e.end_min});
    std::sort(day1.begin(), day1.end());
    for (size_t i = 1; i < day1.size(); ++i) EXPECT_LE(day1[i - 1].second, day1[i].first);
}

TEST(EngineSplitting, MinBreakKeepsTaskSessionsApart) {
    const auto r = run(body(cfg({{"min_break_min", 15}}), {task("a", 60), task("b", 60)}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(9));
    EXPECT_EQ(sessionsOf(r, "b")[0].start, H(10, 15));   // 15-minute break after the first task
}

TEST(EngineSplitting, MaxWorkloadLimitsTaskMinutesPerDay) {
    const json c = cfg({{"max_workload_min", 240}, {"end_date", "2026-11-03"}});
    const auto r = run(body(c, {task("a", 360, {{"allow_split", true}})}));
    EXPECT_VALID(r);
    int day1 = 0, day2 = 0;
    for (const auto& s : sessionsOf(r, "a")) (s.date == kMon ? day1 : day2) += s.end - s.start;
    EXPECT_EQ(day1, 240);
    EXPECT_EQ(day2, 120);
    // non-splittable longer than the daily limit can never be placed
    const auto big = run(body(c, {task("b", 300)}));
    EXPECT_EQ(unscheduled(big, "b")->reason_code, "exceeds_daily_workload");
}

// ===================================================================== G. multi-day

TEST(EngineMultiDay, LaterDayWhenTheFirstIsFull) {
    const auto r = run(body(cfg({{"end_date", "2026-11-03"}}), {task("a", 60)}, {event("e", H(9), H(17))}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a")[0].date, "2026-11-03");
    EXPECT_EQ(sessionsOf(r, "a")[0].start, H(9));
}

TEST(EngineMultiDay, WeekendsAreSkippedWhenOnlyWeekdaysWork) {
    // Fri 2026-11-06 .. Mon 2026-11-09, Mon-Fri only: 600 minutes split -> Fri 480 + Mon 120.
    const json c = cfg({{"target_date", "2026-11-06"}, {"end_date", "2026-11-09"},
                        {"working_weekdays", json::array({"mon", "tue", "wed", "thu", "fri"})}});
    const auto r = run(body(c, {task("a", 600, {{"allow_split", true}})}));
    EXPECT_VALID(r);
    const auto s = sessionsOf(r, "a");
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].date, "2026-11-06");
    EXPECT_EQ(s[1].date, "2026-11-09");
    EXPECT_EQ(r.total_available_min, 2 * 480);
    for (const auto& slot : r.available_slots) EXPECT_TRUE(slot.date == "2026-11-09");   // Fri full, weekend closed
}

TEST(EngineMultiDay, WeekendWorkingWhenConfigured) {
    const json c = cfg({{"target_date", "2026-11-06"}, {"end_date", "2026-11-08"}, {"working_weekdays", json::array({"sat", "sun"})}});
    const auto r = run(body(c, {task("a", 60)}));
    EXPECT_EQ(sessionsOf(r, "a")[0].date, "2026-11-07");   // Friday is not a working day
}

TEST(EngineMultiDay, EndOfHorizonLeavesTheRestUnscheduled) {
    const auto r = run(body(cfg({{"end_date", "2026-11-03"}}), {task("a", 1200, {{"allow_split", true}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(scheduledMinutes(r, "a"), 960);
    const auto* u = unscheduled(r, "a");
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->remaining_min, 240);
    EXPECT_EQ(u->reason_code, "no_available_slot");
    for (const auto& s : sessionsOf(r, "a")) EXPECT_TRUE(s.date <= "2026-11-03");
}

TEST(EngineMultiDay, TaskWhosePrerequisiteEndsAfterTheHorizonIsOutsideIt) {
    const json c = cfg({{"work_window_start_min", 0}, {"work_window_end_min", 1440}});
    const json tasks = json::array({task("A", 1440), task("B", 30, {{"prerequisite_ids", json::array({"A"})}})});
    const auto r = run(body(c, tasks));
    EXPECT_VALID(r);
    EXPECT_EQ(unscheduled(r, "B")->reason_code, "outside_horizon");
}

TEST(EngineMultiDay, DeadlineAtTheEndOfTheHorizon) {
    const auto r = run(body(cfg({{"end_date", "2026-11-03"}}), {task("a", 480), task("b", 480, {{"deadline_date", "2026-11-03"}, {"priority", 1}})}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "b")[0].date, "2026-11-03");
    EXPECT_EQ(r.statistics.tasks_meeting_deadline, 1);
    EXPECT_NE(atRisk(r, "b"), nullptr);   // finishes 17:00 with 7 h of non-working time but no free work time left
}

TEST(EngineMultiDay, ConfigRejectsBadHorizons) {
    EXPECT_THROW(run(body(cfg({{"end_date", "2026-11-01"}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"end_date", "2026-02-30"}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"end_date", "2028-01-01"}}))), models::ValidationError);   // > 366 days
    EXPECT_NO_THROW(run(body(cfg({{"end_date", "2027-11-02"}}))));                            // 366 days
}

TEST(EngineMultiDay, ConfigRejectsBadAvailability) {
    auto win = [](int a, int b) { return json({{"start_min", a}, {"end_min", b}}); };
    EXPECT_THROW(run(body(cfg({{"availability_windows", json::array({win(540, 540)})}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"availability_windows", json::array({win(600, 540)})}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"availability_windows", json::array({win(540, 700), win(650, 800)})}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"availability_windows", json::array({win(-5, 100)})}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"availability_windows", json::array({win(0, 1500)})}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"working_weekdays", json::array({"funday"})}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"working_weekdays", json::array({"mon", "monday"})}}))), models::ValidationError);
    EXPECT_THROW(run(body(cfg({{"objective", "nonsense"}}))), models::ValidationError);
    // touching windows are accepted and merged
    const auto r = run(body(cfg({{"availability_windows", json::array({win(540, 720), win(720, 900)})}}), {task("a", 360)}));
    EXPECT_VALID(r);
    EXPECT_EQ(sessionsOf(r, "a").size(), 1u);
}

TEST(EngineMultiDay, ConfigRoundTripKeepsPhase1KeysAndOnlyAddsSetPhase4Keys) {
    const auto plain = models::SchedulingConfig::fromJson(cfg()).toJson();
    EXPECT_FALSE(plain.contains("end_date"));
    EXPECT_FALSE(plain.contains("availability_windows"));
    const auto rich = models::SchedulingConfig::fromJson(cfg({{"end_date", "2026-11-04"}, {"working_weekdays", json::array({"tue", 0})}})).toJson();
    EXPECT_EQ(rich["end_date"], "2026-11-04");
    EXPECT_EQ(rich["working_weekdays"], json::array({"tue", "mon"}));
}

// ===================================================================== statistics and results

TEST(EngineStatistics, CountsAreConsistentAndNotDoubleCounted) {
    const json tasks = json::array({task("a", 100, {{"allow_split", true}, {"min_chunk_min", 30}}), task("b", 60),
                                    task("c", 2000 > 1440 ? 1440 : 100, {{"allow_split", false}}),
                                    task("d", 10, {{"status", "completed"}})});
    const auto r = run(body(cfg(), tasks, {event("e", H(10), H(11))}));
    EXPECT_VALID(r);
    const auto& s = r.statistics;
    EXPECT_EQ(s.tasks_considered, 3);
    EXPECT_EQ(s.tasks_skipped_terminal, 1);
    EXPECT_EQ(s.tasks_scheduled + s.tasks_partially_scheduled + s.tasks_unscheduled, s.tasks_considered);
    EXPECT_EQ(s.total_requested_min, 100 + 60 + 1440);
    EXPECT_EQ(s.total_scheduled_min + s.total_remaining_min, s.total_requested_min);
    EXPECT_EQ(s.total_scheduled_min, 160);
    int sum = 0;
    for (const auto& e : r.timeline) if (e.type == models::TimelineType::Task) sum += e.end_min - e.start_min;
    EXPECT_EQ(sum, s.total_scheduled_min);
    EXPECT_EQ(r.total_allocated_min, s.total_scheduled_min);
    EXPECT_EQ(s.fixed_events, 1);
    EXPECT_EQ(s.total_window_min, 480);
    EXPECT_EQ(s.total_available_min, 420);
    EXPECT_DOUBLE_EQ(s.utilization_pct, 38.1);   // 160 / 420
}

TEST(EngineResult, AvailableSlotsShowRemainingFreeTime) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("e", H(12), H(13))}));
    ASSERT_EQ(r.available_slots.size(), 2u);
    EXPECT_EQ(r.available_slots[0].start_min, H(10)); EXPECT_EQ(r.available_slots[0].end_min, H(12));
    EXPECT_EQ(r.available_slots[1].start_min, H(13)); EXPECT_EQ(r.available_slots[1].end_min, H(17));
    EXPECT_EQ(r.available_slots[0].id, "slot-1");
    EXPECT_EQ(r.available_slots[0].date, kMon);
}

TEST(EngineResult, ApiJsonKeepsEveryPhase1KeyAndAddsPhase4Keys) {
    const auto r = run(body(cfg(), {task("a", 60)}, {event("e", H(12), H(13))}));
    const json j = r.toApiJson();
    for (const char* k : {"target_date", "status", "solver_duration_ms", "total_allocated_min", "total_available_min",
                          "timeline", "available_slots", "unscheduled_tasks", "tasks_at_risk", "conflicts"})
        EXPECT_TRUE(j.contains(k)) << k;
    for (const char* k : {"start_date", "end_date", "objective", "statistics", "validation_errors", "warnings"})
        EXPECT_TRUE(j.contains(k)) << k;
    json e, firstTask;
    for (const auto& row : j["timeline"]) (row["type"] == "event" ? e : firstTask) = row;
    for (const char* k : {"id", "type", "reference_id", "title", "start", "end", "duration_min", "priority", "status",
                          "explanation", "date", "session_index", "session_count"})
        EXPECT_TRUE(e.contains(k)) << k;
    EXPECT_TRUE(e["session_index"].is_null());            // events have no session numbers
    EXPECT_EQ(firstTask["session_index"], 1);
    EXPECT_EQ(firstTask["session_count"], 1);
    // The Phase 1 serialiser is untouched.
    EXPECT_FALSE(r.toJson().contains("statistics"));
}

TEST(EngineResult, ExplanationsNameTheObjectiveAndSession) {
    const auto r = run(body(cfg({{"objective", "edf"}}), {task("a", 120, {{"allow_split", true}, {"min_chunk_min", 30}})}, {event("e", H(10), H(11))}));
    for (const auto& e : r.timeline)
        if (e.type == models::TimelineType::Task) {
            EXPECT_NE(e.explanation.find("'edf'"), std::string::npos);
            EXPECT_NE(e.explanation.find("Session"), std::string::npos);
        }
}

TEST(EngineResult, ZeroSolverNondeterminismOutsideTheTimingField) {
    const json b = body(cfg({{"end_date", "2026-11-03"}}), {task("a", 300, {{"allow_split", true}}), task("b", 90)}, {event("e", H(11), H(12))});
    json r1 = run(b).toApiJson(), r2 = run(b).toApiJson();
    r1["solver_duration_ms"] = 0; r2["solver_duration_ms"] = 0;
    EXPECT_EQ(r1, r2);
}

// ===================================================================== randomised property test

TEST(EngineProperty, RandomScenariosAlwaysProduceValidSchedulesForEveryObjective) {
    std::mt19937 rng(12345);
    auto rnd = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    for (int iter = 0; iter < 150; ++iter) {
        const int n = rnd(1, 14);
        json tasks = json::array();
        for (int i = 1; i <= n; ++i) {
            json t = task(i, rnd(1, 20) * 15, {{"priority", rnd(1, 5)}, {"allow_split", rnd(0, 1) == 1}});
            if (t["allow_split"].get<bool>()) t["min_chunk_min"] = (std::min)(t["duration_min"].get<int>(), 15 * rnd(1, 4));
            if (rnd(0, 2) == 0) { t["deadline_date"] = formatDay(rnd(0, 4)); t["deadline_min"] = rnd(0, 96) * 15; }
            if (rnd(0, 4) == 0) t["earliest_start_min"] = rnd(0, 40) * 15;
            if (rnd(0, 5) == 0 && t.contains("earliest_start_min") == false) t["latest_completion_min"] = rnd(20, 96) * 15;
            if (rnd(0, 6) == 0) t["status"] = rnd(0, 1) ? "done" : "cancelled";
            if (i > 1) {
                json pre = json::array();
                for (int k = 0; k < rnd(0, 2); ++k) { const int p = rnd(1, i - 1); if (std::find(pre.begin(), pre.end(), json(p)) == pre.end()) pre.push_back(p); }
                t["prerequisite_ids"] = pre;
            }
            tasks.push_back(t);
        }
        json events = json::array();
        for (int i = 0; i < rnd(0, 6); ++i) { const int s = rnd(0, 90) * 15; events.push_back(event(i + 1, s, (std::min)(1440, s + rnd(1, 8) * 15), formatDay(rnd(0, 3)))); }
        for (const char* obj : {"balanced", "priority", "deadline", "compact", "edf"}) {
            json c = cfg({{"objective", obj}, {"end_date", formatDay(3)}, {"min_break_min", rnd(0, 1) * 15}, {"max_workload_min", rnd(0, 1) ? 1440 : 360}});
            const auto r = run(body(c, tasks, events));
            ASSERT_TRUE(r.validation_errors.empty()) << "iter " << iter << " objective " << obj << "\n"
                << body(c, tasks, events).dump() << "\n" << r.toApiJson()["validation_errors"].dump(2);
            EXPECT_EQ(r.statistics.total_scheduled_min + r.statistics.total_remaining_min, r.statistics.total_requested_min);
        }
    }
}

TEST(EnginePerformance, ManyTasksOverAMonthFinishQuicklyAndValidate) {
    json tasks = json::array();
    for (int i = 1; i <= 300; ++i) {
        json t = task(i, 30 + (i % 7) * 15, {{"priority", 1 + i % 5}, {"allow_split", i % 2 == 0}});
        if (i > 3 && i % 3 == 0) t["prerequisite_ids"] = json::array({i - 3});
        tasks.push_back(t);
    }
    json events = json::array();
    for (int d = 0; d < 30; ++d) events.push_back(event(d + 1, H(12), H(13), formatDay(d)));
    const auto r = run(body(cfg({{"end_date", formatDay(29)}, {"objective", "balanced"}}), tasks, events));
    EXPECT_VALID(r);
    EXPECT_LT(r.solver_duration_ms, 5000.0);
    EXPECT_EQ(r.statistics.tasks_considered, 300);
}
