#include <gtest/gtest.h>
#include <set>
#include "models/Event.hpp"
#include "models/Schedule.hpp"
#include "models/Task.hpp"

using namespace chronoplan::models;
using nlohmann::json;

namespace {

json frontendEvent() {
    return {{"id", "evt-1"}, {"title", "Lecture"}, {"date", "2026-10-31"},
            {"start_min", 600}, {"end_min", 660}, {"priority", 4},
            {"is_flexible", false}, {"category", "study"}, {"location", "Room 12"}};
}

json frontendTask() {
    return {{"id", "t-2"}, {"title", "Write report"}, {"priority", 4}, {"duration_min", 90},
            {"deadline_min", 1020}, {"deadline_date", "2026-11-02"}, {"allow_split", true},
            {"prerequisite_ids", json::array({"t-1", 7})}};
}

json validConfig() {
    return {{"target_date", "2026-10-31"}, {"work_window_start_min", 510},
            {"work_window_end_min", 1110}, {"min_break_min", 30}, {"allow_splitting", true},
            {"max_workload_min", 450}, {"objective", "balanced"}};
}

std::set<std::string> keysOf(const json& j) {
    std::set<std::string> k;
    for (auto it = j.begin(); it != j.end(); ++it) k.insert(it.key());
    return k;
}

}  // namespace

// ------------------------------------------------------------- dates, times, ids

TEST(Date, AcceptsValidAndRejectsInvalid) {
    EXPECT_TRUE(isValidDate("2026-10-31"));
    EXPECT_TRUE(isValidDate("2028-02-29"));
    EXPECT_FALSE(isValidDate("2027-02-29"));
    EXPECT_FALSE(isValidDate("2026-13-01"));
    EXPECT_FALSE(isValidDate("2026-1-01"));
    EXPECT_FALSE(isValidDate("not-a-date"));
}

TEST(Time, FormatsHHmm) {
    EXPECT_EQ(formatHHmm(0), "00:00");
    EXPECT_EQ(formatHHmm(510), "08:30");
    EXPECT_EQ(formatHHmm(1110), "18:30");
    EXPECT_EQ(formatHHmm(1440), "24:00");
    EXPECT_THROW(formatHHmm(-1), ValidationError);
    EXPECT_THROW(formatHHmm(1441), ValidationError);
}

TEST(Id, AcceptsStringsAndNumbersAndPreservesType) {
    Id s = parseId(json("abc-1"), "id");
    EXPECT_FALSE(s.numeric);
    EXPECT_EQ(s.toJson(), json("abc-1"));
    Id n = parseId(json(42), "id");
    EXPECT_TRUE(n.numeric);
    EXPECT_EQ(n.toJson(), json(42));
}

TEST(Id, RejectsBadFormats) {
    EXPECT_THROW(parseId(json(""), "id"), ValidationError);
    EXPECT_THROW(parseId(json("has space"), "id"), ValidationError);
    EXPECT_THROW(parseId(json("a/b"), "id"), ValidationError);
    EXPECT_THROW(parseId(json(std::string(65, 'a')), "id"), ValidationError);
    EXPECT_THROW(parseId(json(-3), "id"), ValidationError);
    EXPECT_THROW(parseId(json(1.5), "id"), ValidationError);
    EXPECT_THROW(parseId(json(true), "id"), ValidationError);
    EXPECT_THROW(parseId(json::object(), "id"), ValidationError);
}

// ------------------------------------------------------------------------ events

TEST(Event, ParsesFrontendJson) {                                       // requirement 1
    Event e = Event::fromJson(frontendEvent());
    EXPECT_EQ(e.id.value, "evt-1");
    EXPECT_EQ(e.title, "Lecture");
    EXPECT_EQ(e.date, "2026-10-31");
    EXPECT_EQ(e.start_min, 600);
    EXPECT_EQ(e.end_min, 660);
    EXPECT_EQ(e.priority, 4);
    EXPECT_EQ(e.category, "study");
    EXPECT_EQ(e.location, "Room 12");
    EXPECT_EQ(e.kind, EventKind::Fixed);
}

TEST(Event, IsFlexibleMapsToEventKind) {
    json j = frontendEvent();
    j["is_flexible"] = true;
    EXPECT_EQ(Event::fromJson(j).kind, EventKind::Flexible);
    j.erase("is_flexible");
    EXPECT_EQ(Event::fromJson(j).kind, EventKind::Fixed);   // default
}

TEST(Event, SerialisesBackToFrontendFields) {                           // requirement 3
    json in = frontendEvent();
    json out = Event::fromJson(in).toJson();
    EXPECT_EQ(out, in);
    EXPECT_EQ(keysOf(out), (std::set<std::string>{"id", "title", "date", "start_min", "end_min",
                                                  "priority", "is_flexible", "category", "location"}));
}

TEST(Event, NumericIdSurvivesRoundTrip) {
    json j = frontendEvent();
    j["id"] = 12;
    EXPECT_EQ(Event::fromJson(j).toJson()["id"], json(12));
}

TEST(Event, LegacyKindFieldAcceptedButMustAgree) {
    json j = frontendEvent();
    j.erase("is_flexible");
    j["kind"] = "flexible";
    EXPECT_EQ(Event::fromJson(j).kind, EventKind::Flexible);
    j["is_flexible"] = false;
    EXPECT_THROW(Event::fromJson(j), ValidationError);
}

TEST(Event, RejectsInvalidIntervals) {                                  // requirement 4
    json j = frontendEvent();
    json reversed = j; reversed["start_min"] = 700;
    EXPECT_THROW(Event::fromJson(reversed), ValidationError);
    json zeroLen = j; zeroLen["end_min"] = 600;
    EXPECT_THROW(Event::fromJson(zeroLen), ValidationError);
    json midnight = j; midnight["end_min"] = 1500;
    EXPECT_THROW(Event::fromJson(midnight), ValidationError);
    json negative = j; negative["start_min"] = -5;
    EXPECT_THROW(Event::fromJson(negative), ValidationError);
    json fractional = j; fractional["start_min"] = 600.5;
    EXPECT_THROW(Event::fromJson(fractional), ValidationError);
}

TEST(Event, RejectsOtherBadFields) {
    json j = frontendEvent();
    json badPriority = j; badPriority["priority"] = 0;
    EXPECT_THROW(Event::fromJson(badPriority), ValidationError);
    json badCategory = j; badCategory["category"] = "<script>";
    EXPECT_THROW(Event::fromJson(badCategory), ValidationError);
    json badFlag = j; badFlag["is_flexible"] = "yes";
    EXPECT_THROW(Event::fromJson(badFlag), ValidationError);
    json noDate = j; noDate.erase("date");
    EXPECT_THROW(Event::fromJson(noDate), ValidationError);
    EXPECT_NO_THROW(Event::fromJson(noDate, "2026-10-31"));
}

// ------------------------------------------------------------------------- tasks

TEST(Task, PreservesPrerequisiteIds) {                                  // requirement 2
    Task t = Task::fromJson(frontendTask());
    ASSERT_EQ(t.prerequisite_ids.size(), 2u);
    EXPECT_EQ(t.prerequisite_ids[0].value, "t-1");
    EXPECT_EQ(t.prerequisite_ids[1].value, "7");
    EXPECT_TRUE(t.prerequisite_ids[1].numeric);
    EXPECT_EQ(t.toJson()["prerequisite_ids"], frontendTask()["prerequisite_ids"]);
}

TEST(Task, SerialisesFrontendFields) {                                  // requirement 3
    json out = Task::fromJson(frontendTask()).toJson();
    for (const char* k : {"id", "title", "priority", "duration_min", "deadline_min",
                          "deadline_date", "allow_split", "prerequisite_ids"})
        EXPECT_TRUE(out.contains(k)) << k;
    EXPECT_EQ(out["allow_split"], true);
    EXPECT_EQ(out["deadline_date"], "2026-11-02");
    // And the output must parse again to the same thing.
    EXPECT_EQ(Task::fromJson(out).toJson(), out);
}

TEST(Task, MissingPrerequisitesMeansEmptyList) {
    json j = frontendTask();
    j.erase("prerequisite_ids");
    Task t = Task::fromJson(j);
    EXPECT_TRUE(t.prerequisite_ids.empty());
    EXPECT_TRUE(t.toJson()["prerequisite_ids"].is_array());
}

TEST(Task, RejectsBadPrerequisites) {
    json notArray = frontendTask(); notArray["prerequisite_ids"] = "t-1";
    EXPECT_THROW(Task::fromJson(notArray), ValidationError);
    json badFormat = frontendTask(); badFormat["prerequisite_ids"] = json::array({"ok", "bad id!"});
    EXPECT_THROW(Task::fromJson(badFormat), ValidationError);
    json duplicate = frontendTask(); duplicate["prerequisite_ids"] = json::array({"a", "a"});
    EXPECT_THROW(Task::fromJson(duplicate), ValidationError);
    json selfRef = frontendTask(); selfRef["prerequisite_ids"] = json::array({"t-2"});
    EXPECT_THROW(Task::fromJson(selfRef), ValidationError);
}

TEST(Task, RejectsInvalidDurations) {                                   // requirement 5
    for (const json& bad : {json(0), json(-5), json(1441), json(30.5), json("30")}) {
        json j = frontendTask();
        j["duration_min"] = bad;
        EXPECT_THROW(Task::fromJson(j), ValidationError) << bad.dump();
    }
    json missing = frontendTask(); missing.erase("duration_min");
    EXPECT_THROW(Task::fromJson(missing), ValidationError);
}

TEST(Task, MinChunkDefaultIsValidForVeryShortTasks) {
    for (int d : {1, 2, 4, 5, 20, 30, 90}) {
        json j = {{"title", "x"}, {"duration_min", d}};
        Task t;
        ASSERT_NO_THROW(t = Task::fromJson(j)) << "duration " << d;
        EXPECT_EQ(t.min_chunk_min, std::min(30, d));
        EXPECT_GE(t.min_chunk_min, Task::minChunkLowerBound(d));
    }
}

TEST(Task, ExplicitMinChunkIsChecked) {
    json tooBig = {{"title", "x"}, {"duration_min", 20}, {"min_chunk_min", 25}};
    EXPECT_THROW(Task::fromJson(tooBig), ValidationError);
    json tooSmall = {{"title", "x"}, {"duration_min", 60}, {"min_chunk_min", 4}};
    EXPECT_THROW(Task::fromJson(tooSmall), ValidationError);
    json shortTaskOk = {{"title", "x"}, {"duration_min", 3}, {"min_chunk_min", 3}};
    EXPECT_NO_THROW(Task::fromJson(shortTaskOk));
}

TEST(Task, RejectsOtherBadInput) {
    EXPECT_THROW(Task::fromJson(json{{"title", ""}, {"duration_min", 30}}), ValidationError);
    EXPECT_THROW(Task::fromJson(json{{"title", "x"}, {"priority", 9}, {"duration_min", 30}}), ValidationError);
    EXPECT_THROW(Task::fromJson(json{{"title", 5}, {"duration_min", 30}}), ValidationError);
    EXPECT_THROW(Task::fromJson(json{{"title", "x"}, {"duration_min", 30}, {"deadline_date", "2026-02-30"}}),
                 ValidationError);
    EXPECT_THROW(Task::fromJson(json{{"title", "x"}, {"duration_min", 30}, {"deadline_min", 2000}}),
                 ValidationError);
    EXPECT_THROW(Task::fromJson(json::array()), ValidationError);
}

// ------------------------------------------------- full task model (all fields)

namespace {
json fullTask() {
    return {{"id", "t-2"}, {"title", "Write report"},
            {"description", "Draft the final report"}, {"technical_context", "Uses the LaTeX template"},
            {"priority", 4}, {"duration_min", 90}, {"min_chunk_min", 30}, {"allow_split", true},
            {"deadline_date", "2026-11-02"}, {"deadline_min", 1020},
            {"earliest_start_min", 540}, {"latest_completion_min", 1000},
            {"prerequisite_ids", json::array({"t-1", 7})}, {"status", "in_progress"}};
}
}  // namespace

TEST(Task, AllFieldsRoundTripUnchanged) {
    json in = fullTask();
    json out = Task::fromJson(in).toJson();
    EXPECT_EQ(out, in);
    EXPECT_EQ(keysOf(out).size(), 14u);
}

TEST(Task, MissingOptionalFieldsAreNullNotInvented) {
    Task t = Task::fromJson(json{{"title", "x"}, {"duration_min", 30}});
    EXPECT_FALSE(t.description);
    EXPECT_FALSE(t.technical_context);
    EXPECT_FALSE(t.deadline_date);
    EXPECT_FALSE(t.earliest_start_min);
    EXPECT_FALSE(t.latest_completion_min);
    EXPECT_FALSE(t.status);
    EXPECT_TRUE(t.prerequisite_ids.empty());
    json out = t.toJson();
    for (const char* k : {"id", "description", "technical_context", "deadline_date",
                          "earliest_start_min", "latest_completion_min", "status"}) {
        ASSERT_TRUE(out.contains(k)) << k;
        EXPECT_TRUE(out[k].is_null()) << k;
    }
    EXPECT_TRUE(out["prerequisite_ids"].is_array());
    // A null round-trips back to "absent".
    EXPECT_EQ(Task::fromJson(out).toJson(), out);
}

TEST(Task, NumericAndStringIdsKeepTheirType) {
    json j = fullTask();
    j["id"] = 5;
    json out = Task::fromJson(j).toJson();
    EXPECT_TRUE(out["id"].is_number_integer());
    EXPECT_EQ(out["id"], 5);
    EXPECT_TRUE(out["prerequisite_ids"][1].is_number_integer());
    EXPECT_TRUE(out["prerequisite_ids"][0].is_string());
}

TEST(Task, StatusIsPreservedNotCollapsedToBoolean) {
    for (const char* s : {"pending", "in_progress", "done", "In Progress", "blocked-2"}) {
        json j = fullTask(); j["status"] = s;
        EXPECT_EQ(Task::fromJson(j).toJson()["status"], s) << s;
    }
}

TEST(Task, RejectsInvalidStatus) {
    for (const json& bad : {json(""), json("bad!status"), json(std::string(33, 'a')), json(5), json(true)}) {
        json j = fullTask(); j["status"] = bad;
        EXPECT_THROW(Task::fromJson(j), ValidationError) << bad.dump();
    }
}

TEST(Task, RejectsInvalidEarliestStart) {
    for (const json& bad : {json(-1), json(1440), json(2000), json(600.5), json("09:00")}) {
        json j = fullTask(); j["earliest_start_min"] = bad;
        EXPECT_THROW(Task::fromJson(j), ValidationError) << bad.dump();
    }
}

TEST(Task, RejectsInvalidLatestCompletion) {
    for (const json& bad : {json(0), json(-5), json(1441), json("17:00")}) {
        json j = fullTask(); j["latest_completion_min"] = bad;
        EXPECT_THROW(Task::fromJson(j), ValidationError) << bad.dump();
    }
}

TEST(Task, RejectsLatestCompletionNotAfterEarliestStart) {
    json before = fullTask(); before["earliest_start_min"] = 900; before["latest_completion_min"] = 600;
    EXPECT_THROW(Task::fromJson(before), ValidationError);
    json equal = fullTask(); equal["earliest_start_min"] = 600; equal["latest_completion_min"] = 600;
    EXPECT_THROW(Task::fromJson(equal), ValidationError);
    json after = fullTask(); after["earliest_start_min"] = 600; after["latest_completion_min"] = 1000;
    EXPECT_NO_THROW(Task::fromJson(after));
}

TEST(Task, EitherBoundAloneIsAccepted) {
    json onlyEarliest = fullTask(); onlyEarliest.erase("latest_completion_min");
    EXPECT_NO_THROW(Task::fromJson(onlyEarliest));
    json onlyLatest = fullTask(); onlyLatest.erase("earliest_start_min");
    EXPECT_NO_THROW(Task::fromJson(onlyLatest));
}

TEST(Task, WindowShorterThanDurationIsInputNotAnError) {
    // The scheduler must report this honestly later; the parser must not reject it.
    json j = fullTask(); j["earliest_start_min"] = 600; j["latest_completion_min"] = 630;
    EXPECT_NO_THROW(Task::fromJson(j));
}

TEST(Task, TextFieldsAreTypeAndLengthChecked) {
    json tooLong = fullTask(); tooLong["description"] = std::string(2001, 'x');
    EXPECT_THROW(Task::fromJson(tooLong), ValidationError);
    json wrongType = fullTask(); wrongType["technical_context"] = 12;
    EXPECT_THROW(Task::fromJson(wrongType), ValidationError);
}

// ----------------------------------------------------------- config and request

TEST(Config, AcceptsContractSample) {
    EXPECT_NO_THROW(SchedulingConfig::fromJson(validConfig()));
}

TEST(Config, RejectsInvalidBoundaries) {
    auto with = [](const char* key, json value) {
        json c = validConfig();
        c[key] = value;
        return c;
    };
    EXPECT_THROW(SchedulingConfig::fromJson(with("work_window_start_min", 1200)), ValidationError);  // start > end
    EXPECT_THROW(SchedulingConfig::fromJson(with("work_window_start_min", 1110)), ValidationError);  // start == end
    EXPECT_THROW(SchedulingConfig::fromJson(with("work_window_start_min", -1)), ValidationError);
    EXPECT_THROW(SchedulingConfig::fromJson(with("work_window_end_min", 1500)), ValidationError);
    EXPECT_THROW(SchedulingConfig::fromJson(with("min_break_min", -5)), ValidationError);
    EXPECT_THROW(SchedulingConfig::fromJson(with("min_break_min", 600)), ValidationError);
    EXPECT_THROW(SchedulingConfig::fromJson(with("max_workload_min", 0)), ValidationError);
    EXPECT_THROW(SchedulingConfig::fromJson(with("target_date", "2026-02-30")), ValidationError);
}

TEST(Config, ValidatesObjective) {
    for (const char* ok : {"balanced", "priority", "deadline", "compact"}) {
        json c = validConfig(); c["objective"] = ok;
        EXPECT_NO_THROW(SchedulingConfig::fromJson(c)) << ok;
    }
    json bad = validConfig(); bad["objective"] = "fastest";
    EXPECT_THROW(SchedulingConfig::fromJson(bad), ValidationError);
    json empty = validConfig(); empty["objective"] = "";
    EXPECT_THROW(SchedulingConfig::fromJson(empty), ValidationError);
}

TEST(Request, ParsesFullFrontendPayload) {
    json body = {{"version", "1.0"}, {"config", validConfig()},
                 {"fixed_events", json::array({frontendEvent()})},
                 {"flexible_tasks", json::array({frontendTask()})}};
    SchedulingRequest r = SchedulingRequest::fromJson(body);
    ASSERT_EQ(r.fixed_events.size(), 1u);
    ASSERT_EQ(r.flexible_tasks.size(), 1u);
    EXPECT_EQ(r.flexible_tasks[0].prerequisite_ids.size(), 2u);
}

TEST(Request, RejectsStructuralProblems) {
    EXPECT_THROW(SchedulingRequest::fromJson(json{{"version", "1.0"}}), ValidationError);  // no config
    json notArray = {{"config", validConfig()}, {"fixed_events", 5}};
    EXPECT_THROW(SchedulingRequest::fromJson(notArray), ValidationError);
    json dupTasks = {{"config", validConfig()},
                     {"flexible_tasks", json::array({frontendTask(), frontendTask()})}};
    EXPECT_THROW(SchedulingRequest::fromJson(dupTasks), ValidationError);
    json badTask = frontendTask(); badTask["duration_min"] = 0;
    json oneBad = {{"config", validConfig()}, {"flexible_tasks", json::array({badTask})}};
    EXPECT_THROW(SchedulingRequest::fromJson(oneBad), ValidationError);
}

// ------------------------------------------------------------------------ result

namespace {
TimelineEntry sampleTaskEntry() {
    TimelineEntry te;
    te.id = "tl-1"; te.type = TimelineType::Task; te.reference_id = parseId(json("t-2"), "id");
    te.title = "Write report"; te.start_min = 510; te.end_min = 600; te.priority = 4;
    te.status = TimelineStatus::Scheduled; te.explanation = "Highest-priority eligible task";
    return te;
}
}  // namespace

TEST(Result, EnumStringsMatchDocumentedValues) {
    EXPECT_STREQ(toString(TimelineType::Event), "event");
    EXPECT_STREQ(toString(TimelineType::Task), "task");
    EXPECT_STREQ(toString(TimelineStatus::Fixed), "fixed");
    EXPECT_STREQ(toString(TimelineStatus::Scheduled), "scheduled");
    EXPECT_STREQ(toString(ResultStatus::Success), "success");
    EXPECT_STREQ(toString(ResultStatus::Partial), "partial");
    EXPECT_STREQ(toString(ResultStatus::Infeasible), "infeasible");
    EXPECT_STREQ(toString(RiskLevel::OnTrack), "on_track");
    EXPECT_STREQ(toString(RiskLevel::AtRisk), "at_risk");
    EXPECT_STREQ(toString(RiskLevel::ImpossibleWithoutRescheduling), "impossible_without_rescheduling");
}

TEST(Result, TopLevelFieldsAlwaysPresent) {
    json j = SchedulingResult{}.toJson();
    EXPECT_EQ(keysOf(j), (std::set<std::string>{"target_date", "status", "solver_duration_ms",
        "total_allocated_min", "total_available_min", "timeline", "available_slots",
        "unscheduled_tasks", "tasks_at_risk", "conflicts"}));
    EXPECT_EQ(j["status"], "success");
    for (const char* k : {"timeline", "available_slots", "unscheduled_tasks", "tasks_at_risk", "conflicts"})
        EXPECT_TRUE(j[k].is_array()) << k;
}

TEST(Result, TimelineTaskEntrySerialisation) {
    json j = sampleTaskEntry().toJson();
    EXPECT_EQ(keysOf(j), (std::set<std::string>{"id", "type", "reference_id", "title", "start", "end",
                                                "duration_min", "priority", "status", "explanation"}));
    EXPECT_EQ(j["id"], "tl-1");
    EXPECT_EQ(j["type"], "task");
    EXPECT_EQ(j["reference_id"], "t-2");
    EXPECT_EQ(j["start"], "08:30");
    EXPECT_EQ(j["end"], "10:00");
    EXPECT_EQ(j["duration_min"], 90);
    EXPECT_TRUE(j["priority"].is_number_integer());
    EXPECT_EQ(j["status"], "scheduled");
    EXPECT_EQ(j["explanation"], "Highest-priority eligible task");
}

TEST(Result, TimelineFixedEventEntryWithNumericReference) {
    TimelineEntry te = sampleTaskEntry();
    te.id = "tl-2"; te.type = TimelineType::Event; te.status = TimelineStatus::Fixed;
    te.reference_id = parseId(json(12), "id");
    te.start_min = 600; te.end_min = 660; te.priority = 5;
    json j = te.toJson();
    EXPECT_EQ(j["type"], "event");
    EXPECT_EQ(j["status"], "fixed");
    EXPECT_TRUE(j["reference_id"].is_number_integer());
    EXPECT_EQ(j["reference_id"], 12);
}

TEST(Result, TimelineRejectsOutOfRangeMinutes) {
    TimelineEntry te = sampleTaskEntry();
    te.end_min = 1500;
    EXPECT_THROW(te.toJson(), ValidationError);
}

TEST(Result, AvailableSlotsHaveIds) {
    Slot s; s.id = "slot-1"; s.start_min = 600; s.end_min = 660; s.label = "Free";
    json j = s.toJson();
    EXPECT_EQ(keysOf(j), (std::set<std::string>{"id", "start", "end", "duration_min", "label"}));
    EXPECT_EQ(j["id"], "slot-1");
    EXPECT_EQ(j["start"], "10:00");
    EXPECT_EQ(j["end"], "11:00");
    EXPECT_EQ(j["duration_min"], 60);
}

TEST(Result, UnscheduledTaskFields) {
    UnscheduledTask u;
    u.task_id = parseId(json("t-9"), "id"); u.title = "Big task"; u.remaining_min = 120;
    u.reason = "No free interval long enough"; u.suggested_action = "Allow splitting";
    json j = u.toJson();
    for (const char* k : {"task_id", "title", "reason", "suggested_action"}) EXPECT_TRUE(j.contains(k)) << k;
    EXPECT_EQ(j["task_id"], "t-9");
}

TEST(Result, TaskAtRiskFields) {
    TaskAtRisk a;
    a.task_id = parseId(json(5), "id"); a.title = "Essay";
    a.risk_level = RiskLevel::AtRisk; a.buffer_min = -15; a.reason = "Only 45 min free";
    json j = a.toJson();
    for (const char* k : {"task_id", "title", "risk_level", "buffer_min", "reason"})
        EXPECT_TRUE(j.contains(k)) << k;
    EXPECT_EQ(j["risk_level"], "at_risk");
    EXPECT_EQ(j["buffer_min"], -15);
    EXPECT_TRUE(j["task_id"].is_number_integer());
}

TEST(Result, ConflictHasIdAndHHmmOverlap) {
    Conflict c;
    c.id = "cf-1"; c.event_a_id = parseId(json("e-1"), "id"); c.event_b_id = parseId(json(2), "id");
    c.date = "2026-10-31"; c.overlap_start_min = 615; c.overlap_end_min = 630; c.message = "Overlap";
    json j = c.toJson();
    EXPECT_EQ(j["id"], "cf-1");
    EXPECT_EQ(j["event_a_id"], "e-1");
    EXPECT_EQ(j["event_b_id"], 2);
    EXPECT_EQ(j["overlap_start"], "10:15");
    EXPECT_EQ(j["overlap_end"], "10:30");
}

TEST(Result, FullResultSerialisesNestedArrays) {
    SchedulingResult r;
    r.target_date = "2026-10-31"; r.status = ResultStatus::Partial;
    r.timeline.push_back(sampleTaskEntry());
    Slot s; s.id = "slot-1"; s.start_min = 600; s.end_min = 660; s.label = "Free";
    r.available_slots.push_back(s);
    json j = r.toJson();
    EXPECT_EQ(j["status"], "partial");
    ASSERT_EQ(j["timeline"].size(), 1u);
    ASSERT_EQ(j["available_slots"].size(), 1u);
    EXPECT_EQ(j["available_slots"][0]["id"], "slot-1");
    EXPECT_TRUE(j["conflicts"].empty());
}

// ------------------------------------------- id identity: JSON type matters

TEST(Id, EqualityDistinguishesNumericFromString) {
    const Id s7 = parseId(json("7"), "id");
    const Id s7b = parseId(json("7"), "id");
    const Id n7 = parseId(json(7), "id");
    const Id n7b = parseId(json(7), "id");
    EXPECT_FALSE(n7 == s7);      // numeric 7 != string "7"
    EXPECT_FALSE(s7 == n7);
    EXPECT_TRUE(n7 == n7b);      // numeric 7 == numeric 7
    EXPECT_TRUE(s7 == s7b);      // string "7" == string "7"
    EXPECT_NE(n7.key(), s7.key());
    EXPECT_EQ(n7.key(), n7b.key());
    EXPECT_EQ(s7.key(), s7b.key());
    EXPECT_FALSE(parseId(json("7"), "id") == parseId(json("8"), "id"));
}

TEST(Task, NumericAndStringPrerequisiteWithSameTextAreDistinct) {
    json mixed = {{"title", "x"}, {"duration_min", 30}, {"prerequisite_ids", json::array({"7", 7})}};
    Task t;
    ASSERT_NO_THROW(t = Task::fromJson(mixed));
    ASSERT_EQ(t.prerequisite_ids.size(), 2u);
    EXPECT_EQ(t.toJson()["prerequisite_ids"], mixed["prerequisite_ids"]);   // both kept, types intact
}

TEST(Task, RejectsDuplicatePrerequisitesOfTheSameType) {
    json numeric = {{"title", "x"}, {"duration_min", 30}, {"prerequisite_ids", json::array({7, 7})}};
    EXPECT_THROW(Task::fromJson(numeric), ValidationError);
    json str = {{"title", "x"}, {"duration_min", 30}, {"prerequisite_ids", json::array({"7", "7"})}};
    EXPECT_THROW(Task::fromJson(str), ValidationError);
}

TEST(Task, SelfPrerequisiteCheckIsTypeAware) {
    json numericSelf = {{"id", 7}, {"title", "x"}, {"duration_min", 30},
                        {"prerequisite_ids", json::array({7})}};
    EXPECT_THROW(Task::fromJson(numericSelf), ValidationError);
    json stringSelf = {{"id", "7"}, {"title", "x"}, {"duration_min", 30},
                       {"prerequisite_ids", json::array({"7"})}};
    EXPECT_THROW(Task::fromJson(stringSelf), ValidationError);
    json differentType = {{"id", 7}, {"title", "x"}, {"duration_min", 30},
                          {"prerequisite_ids", json::array({"7"})}};
    EXPECT_NO_THROW(Task::fromJson(differentType));
}

TEST(Request, DuplicateIdDetectionIsTypeAware) {
    json numeric = {{"id", 7}, {"title", "a"}, {"duration_min", 30}};
    json str = {{"id", "7"}, {"title", "b"}, {"duration_min", 30}};
    json ok = {{"config", validConfig()}, {"flexible_tasks", json::array({numeric, str})}};
    EXPECT_NO_THROW(SchedulingRequest::fromJson(ok));
    json dupNumeric = {{"config", validConfig()}, {"flexible_tasks", json::array({numeric, numeric})}};
    EXPECT_THROW(SchedulingRequest::fromJson(dupNumeric), ValidationError);
    json dupString = {{"config", validConfig()}, {"flexible_tasks", json::array({str, str})}};
    EXPECT_THROW(SchedulingRequest::fromJson(dupString), ValidationError);

    json evNum = frontendEvent(); evNum["id"] = 7;
    json evStr = frontendEvent(); evStr["id"] = "7";
    json evOk = {{"config", validConfig()}, {"fixed_events", json::array({evNum, evStr})}};
    EXPECT_NO_THROW(SchedulingRequest::fromJson(evOk));
    json evDup = {{"config", validConfig()}, {"fixed_events", json::array({evNum, evNum})}};
    EXPECT_THROW(SchedulingRequest::fromJson(evDup), ValidationError);
}
