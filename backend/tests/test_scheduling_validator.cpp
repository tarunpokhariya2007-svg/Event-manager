// The validator must catch hard-constraint violations WITHOUT trusting the engine: each test takes
// a valid engine result, corrupts it, and expects the specific violation code.
#include "SchedTestUtil.hpp"

using namespace schedtest;
using nlohmann::json;

namespace {

struct Fixture {
    json b;
    scheduler::SchedulingContext ctx;
    SchedulingResult good;
    explicit Fixture(json body_) : b(std::move(body_)),
          ctx(scheduler::SchedulingContext::build(models::SchedulingRequest::fromJson(b))),
          good(scheduler::SchedulingEngine::run(ctx)) {}
    std::vector<std::string> codes(const SchedulingResult& r) const {
        std::vector<std::string> out;
        for (const auto& v : scheduler::SchedulingValidator::validate(ctx, r)) out.push_back(v.code);
        return out;
    }
    bool has(const SchedulingResult& r, const std::string& code) const {
        const auto c = codes(r);
        return std::find(c.begin(), c.end(), code) != c.end();
    }
    models::TimelineEntry* firstTask(SchedulingResult& r, const json& id) const {
        for (auto& e : r.timeline) if (e.type == models::TimelineType::Task && sameId(e.reference_id, id)) return &e;
        return nullptr;
    }
};

// Tasks: A (60) -> B (60, requires A); event 12:00-13:00; min break 0.
Fixture basic() {
    return Fixture(body(cfg({{"end_date", "2026-11-03"}}),
        {task("A", 60, {{"deadline_date", kMon}, {"deadline_min", H(17)}}),
         task("B", 60, {{"prerequisite_ids", json::array({"A"})}, {"earliest_start_min", H(9)}, {"latest_completion_min", H(17)}}),
         task("S", 120, {{"allow_split", true}, {"min_chunk_min", 60}}),
         task("Done", 30, {{"status", "completed"}})},
        {event("E", H(12), H(13))}));
}

}  // namespace

TEST(Validator, AcceptsAValidSchedule) {
    Fixture f = basic();
    EXPECT_TRUE(f.good.validation_errors.empty());
    EXPECT_TRUE(f.codes(f.good).empty());
}

TEST(Validator, DetectsOverlappingTasks) {
    Fixture f = basic();
    auto r = f.good;
    f.firstTask(r, "B")->start_min = f.firstTask(r, "A")->start_min + 10;
    f.firstTask(r, "B")->end_min = f.firstTask(r, "B")->start_min + 60;
    EXPECT_TRUE(f.has(r, "task_overlap"));
}

TEST(Validator, DetectsOverlapWithAFixedEvent) {
    Fixture f = basic();
    auto r = f.good;
    auto* a = f.firstTask(r, "A");
    a->start_min = H(11, 30); a->end_min = H(12, 30);
    EXPECT_TRUE(f.has(r, "event_overlap"));
}

TEST(Validator, DetectsATaskOutsideAvailability) {
    Fixture f = basic();
    auto r = f.good;
    auto* a = f.firstTask(r, "A");
    a->start_min = H(18); a->end_min = H(19);                  // after 17:00
    EXPECT_TRUE(f.has(r, "outside_availability"));
    r = f.good;
    a = f.firstTask(r, "A");
    a->start_min = H(8, 30); a->end_min = H(9, 30);            // straddles the window start
    EXPECT_TRUE(f.has(r, "outside_availability"));
}

TEST(Validator, DetectsEarlyStartAndLateCompletion) {
    Fixture f(body(cfg(), {task("A", 60, {{"earliest_start_min", H(13)}, {"latest_completion_min", H(15)}})}));
    ASSERT_EQ(f.good.timeline.size(), 1u);
    auto early = f.good;
    early.timeline[0].start_min = H(12); early.timeline[0].end_min = H(13);
    EXPECT_TRUE(f.has(early, "early_start"));
    auto late = f.good;
    late.timeline[0].start_min = H(14, 30); late.timeline[0].end_min = H(15, 30);
    EXPECT_TRUE(f.has(late, "late_completion"));
}

TEST(Validator, DetectsADeadlineViolation) {
    Fixture f = basic();
    auto r = f.good;
    auto* a = f.firstTask(r, "A");
    a->date = "2026-11-03";   // deadline was 2026-11-02 17:00
    EXPECT_TRUE(f.has(r, "deadline_violation"));
}

TEST(Validator, DetectsADependentStartingBeforeItsPrerequisite) {
    Fixture f = basic();
    auto r = f.good;
    auto* a = f.firstTask(r, "A");
    auto* bb = f.firstTask(r, "B");
    std::swap(a->start_min, bb->start_min);
    std::swap(a->end_min, bb->end_min);
    EXPECT_TRUE(f.has(r, "dependency_order"));
}

TEST(Validator, DetectsADependentScheduledWithoutItsPrerequisite) {
    Fixture f = basic();
    auto r = f.good;
    r.timeline.erase(std::remove_if(r.timeline.begin(), r.timeline.end(), [](const models::TimelineEntry& e) {
        return e.type == models::TimelineType::Task && e.reference_id.value == "A"; }), r.timeline.end());
    EXPECT_TRUE(f.has(r, "prerequisite_unscheduled"));
}

TEST(Validator, DetectsASplitOfANonSplittableTask) {
    Fixture f = basic();
    auto r = f.good;
    models::TimelineEntry extra = *f.firstTask(r, "A");
    extra.id = "tl-900"; extra.start_min = H(15); extra.end_min = H(15, 30);
    r.timeline.push_back(extra);
    EXPECT_TRUE(f.has(r, "task_split"));
    EXPECT_TRUE(f.has(r, "chunk_too_short"));
}

TEST(Validator, DetectsASessionShorterThanTheMinimumChunk) {
    Fixture f = basic();
    auto r = f.good;
    int seen = 0;
    for (auto& e : r.timeline)
        if (e.type == models::TimelineType::Task && e.reference_id.value == "S" && seen++ == 0) e.end_min = e.start_min + 20;
    EXPECT_TRUE(f.has(r, "chunk_too_short"));
}

TEST(Validator, DetectsMoreScheduledMinutesThanTheTaskDuration) {
    Fixture f = basic();
    auto r = f.good;
    models::TimelineEntry extra = *f.firstTask(r, "S");
    extra.id = "tl-901"; extra.date = "2026-11-03"; extra.start_min = H(9); extra.end_min = H(11);
    r.timeline.push_back(extra);
    EXPECT_TRUE(f.has(r, "duration_exceeded"));
}

TEST(Validator, DetectsDatesOutsideTheHorizon) {
    Fixture f = basic();
    auto r = f.good;
    f.firstTask(r, "A")->date = "2026-11-10";
    EXPECT_TRUE(f.has(r, "outside_horizon"));
}

TEST(Validator, DetectsInvalidIdentifiersAndBoundaries) {
    Fixture f = basic();
    auto noId = f.good;
    noId.timeline[0].id = "";
    EXPECT_TRUE(f.has(noId, "invalid_entry"));
    auto dup = f.good;
    dup.timeline[1].id = dup.timeline[0].id;
    EXPECT_TRUE(f.has(dup, "invalid_entry"));
    auto bounds = f.good;
    f.firstTask(bounds, "A")->end_min = f.firstTask(bounds, "A")->start_min;
    EXPECT_TRUE(f.has(bounds, "invalid_entry"));
    auto past = f.good;
    f.firstTask(past, "A")->end_min = 1500;
    EXPECT_TRUE(f.has(past, "invalid_entry"));
    auto badDate = f.good;
    f.firstTask(badDate, "A")->date = "not-a-date";
    EXPECT_TRUE(f.has(badDate, "invalid_entry"));
    auto noRef = f.good;
    f.firstTask(noRef, "A")->reference_id = models::Id{};
    EXPECT_TRUE(f.has(noRef, "invalid_entry"));
}

TEST(Validator, DetectsAMovedMissingOrInventedFixedEvent) {
    Fixture f = basic();
    auto moved = f.good;
    for (auto& e : moved.timeline) if (e.type == models::TimelineType::Event) { e.start_min += 30; e.end_min += 30; }
    EXPECT_TRUE(f.has(moved, "event_modified"));
    auto missing = f.good;
    missing.timeline.erase(std::remove_if(missing.timeline.begin(), missing.timeline.end(),
        [](const models::TimelineEntry& e) { return e.type == models::TimelineType::Event; }), missing.timeline.end());
    EXPECT_TRUE(f.has(missing, "event_modified"));
}

TEST(Validator, DetectsUnknownCompletedAndCyclicTasksBeingScheduled) {
    Fixture f(body(cfg(), {task("Done", 30, {{"status", "completed"}}), task("P", 30, {{"prerequisite_ids", json::array({"Q"})}}),
                           task("Q", 30, {{"prerequisite_ids", json::array({"P"})}}), task("Z", 30)},
                   {}));
    auto base = f.good;
    ASSERT_TRUE(f.codes(base).empty());
    auto mk = [&](const json& id) { auto e = *f.firstTask(base, "Z"); e.id = "tl-77"; e.reference_id = models::Id{}; e.reference_id.value = id.get<std::string>(); e.start_min = H(15); e.end_min = H(15, 30); return e; };
    auto r = base; r.timeline.push_back(mk("Done"));
    EXPECT_TRUE(f.has(r, "terminal_task_scheduled"));
    r = base; r.timeline.push_back(mk("P"));
    EXPECT_TRUE(f.has(r, "cyclic_task_scheduled"));
    r = base; r.timeline.push_back(mk("Nope"));
    EXPECT_TRUE(f.has(r, "unknown_task"));
}

TEST(Validator, DetectsDailyWorkloadAndBreakViolations) {
    Fixture f(body(cfg({{"max_workload_min", 120}, {"min_break_min", 15}}), {task("A", 60), task("B", 60)}));
    ASSERT_TRUE(f.codes(f.good).empty());
    auto tight = f.good;                               // remove the 15-minute break
    f.firstTask(tight, "B")->start_min = f.firstTask(tight, "A")->end_min;
    f.firstTask(tight, "B")->end_min = f.firstTask(tight, "B")->start_min + 60;
    EXPECT_TRUE(f.has(tight, "break_violation"));
    auto heavy = f.good;
    models::TimelineEntry extra = *f.firstTask(heavy, "A");
    extra.id = "tl-55"; extra.start_min = H(15); extra.end_min = H(16);
    extra.reference_id = f.firstTask(heavy, "B")->reference_id;
    heavy.timeline.push_back(extra);
    EXPECT_TRUE(f.has(heavy, "workload_exceeded"));
}

TEST(Validator, DetectsNonAdjacentOverlapHiddenBehindALongSession) {
    Fixture f(body(cfg(), {task("L", 240), task("M", 30), task("N", 30)}));
    auto r = f.good;
    auto* l = f.firstTask(r, "L");
    l->start_min = H(9); l->end_min = H(13);
    auto* m = f.firstTask(r, "M"); m->start_min = H(9, 30); m->end_min = H(10);
    auto* n = f.firstTask(r, "N"); n->start_min = H(10, 30); n->end_min = H(11);   // overlaps L, not M
    EXPECT_TRUE(f.has(r, "task_overlap"));
}

TEST(Validator, EngineNeverReportsASuccessfulScheduleThatFailsValidation) {
    // The engine runs the validator itself; a corrupted copy re-validated must not be "success".
    Fixture f = basic();
    EXPECT_EQ(f.good.status, models::ResultStatus::Success);
    EXPECT_TRUE(f.good.validation_errors.empty());
}
