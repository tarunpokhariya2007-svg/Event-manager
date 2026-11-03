#include <gtest/gtest.h>
#include "TestDb.hpp"
#include "repositories/EventRepository.hpp"
#include "repositories/RepositoryErrors.hpp"

using namespace chronoplan;
using namespace chronoplan::repositories;
using nlohmann::json;
using testutil::makeEvent;
using testutil::num;
using testutil::str;

namespace {
struct Fixture : ::testing::Test {
    testutil::TempDb tmp;
    database::DatabaseManager mgr{tmp.config()};
    long long uid = testutil::makeUser(mgr.database());
    EventRepository repo{mgr.database(), uid};
};
}  // namespace

TEST_F(Fixture, CreateAndRetrieveRoundTripsEveryField) {
    const auto e = makeEvent(4, "2026-11-02", 540, 600,
        {{"title", "Standup"}, {"priority", 4}, {"is_flexible", true}, {"category", "work"}, {"location", "Room 5"}});
    repo.createEvent(e);
    const auto got = repo.getEventById(num(4));
    ASSERT_TRUE(got);
    EXPECT_EQ(got->toJson(), e.toJson());
    EXPECT_TRUE(got->isFlexible());
}

TEST_F(Fixture, OptionalFieldsDefaultToEmpty) {
    repo.createEvent(makeEvent(1));
    const auto got = repo.getEventById(num(1));
    EXPECT_EQ(got->category, "");
    EXPECT_EQ(got->location, "");
    EXPECT_FALSE(got->isFlexible());
    EXPECT_EQ(got->priority, 3);
}

TEST_F(Fixture, GetAllAndMissing) {
    repo.createEvent(makeEvent(2));
    repo.createEvent(makeEvent("e"));
    EXPECT_EQ(repo.getAllEvents().size(), 2u);
    EXPECT_FALSE(repo.getEventById(num(99)));
    EXPECT_THROW(repo.updateEvent(num(99), makeEvent(99)), NotFoundError);
    EXPECT_THROW(repo.deleteEvent(num(99)), NotFoundError);
}

TEST_F(Fixture, UpdateReplacesAllFields) {
    repo.createEvent(makeEvent(1, "2026-11-02", 540, 600, {{"category", "work"}, {"location", "HQ"}}));
    repo.updateEvent(num(1), makeEvent(nullptr, "2026-11-03", 60, 120, {{"title", "Gym"}}));
    const auto got = repo.getEventById(num(1));
    EXPECT_EQ(got->title, "Gym");
    EXPECT_EQ(got->date, "2026-11-03");
    EXPECT_EQ(got->start_min, 60);
    EXPECT_EQ(got->category, "");
    EXPECT_EQ(got->location, "");
}

TEST_F(Fixture, DeleteRemovesEvent) {
    repo.createEvent(makeEvent(1));
    repo.deleteEvent(num(1));
    EXPECT_FALSE(repo.eventExists(num(1)));
}

TEST_F(Fixture, DuplicateIdsAndTypedIds) {
    repo.createEvent(makeEvent(7));
    EXPECT_THROW(repo.createEvent(makeEvent(7)), ConflictError);
    EXPECT_NO_THROW(repo.createEvent(makeEvent("7")));
    EXPECT_TRUE(repo.getEventById(str("7"))->toJson()["id"].is_string());
    EXPECT_TRUE(repo.getEventById(num(7))->toJson()["id"].is_number_integer());
}

TEST_F(Fixture, AssignsNumericIdWhenOmitted) {
    EXPECT_EQ(repo.createEvent(makeEvent(nullptr)).id.value, "1");
    EXPECT_EQ(repo.createEvent(makeEvent(nullptr)).id.value, "2");
}

TEST_F(Fixture, FilterByDateIsExactAndOrderedByStart) {
    repo.createEvent(makeEvent(1, "2026-11-02", 780, 840));
    repo.createEvent(makeEvent(2, "2026-11-02", 540, 600));
    repo.createEvent(makeEvent(3, "2026-11-03", 540, 600));
    const auto day = repo.getEventsByDate("2026-11-02");
    ASSERT_EQ(day.size(), 2u);
    EXPECT_EQ(day[0].id, num(2));
    EXPECT_EQ(day[1].id, num(1));
    EXPECT_TRUE(repo.getEventsByDate("2030-01-01").empty());
}

TEST_F(Fixture, InvalidDateAndInvalidEventRejected) {
    EXPECT_THROW(repo.getEventsByDate("2026-13-01"), models::ValidationError);
    EXPECT_THROW(repo.getEventsByDate("not-a-date"), models::ValidationError);
    EXPECT_THROW(repo.getEventsByDate(""), models::ValidationError);
    models::Event bad = makeEvent(1);
    bad.end_min = bad.start_min;
    EXPECT_THROW(repo.createEvent(bad), models::ValidationError);
    EXPECT_FALSE(repo.eventExists(num(1)));
}

TEST(EventRepositoryReopen, PersistsAfterReopening) {
    testutil::TempDb tmp;
    {
        database::DatabaseManager m(tmp.config());
        EventRepository r(m.database(), testutil::makeUser(m.database()));
        r.createEvent(makeEvent(1, "2026-11-02", 540, 600, {{"category", "work"}, {"location", "HQ"}, {"is_flexible", true}}));
    }
    database::DatabaseManager m(tmp.config());
    EventRepository r(m.database(), 1);   // the first inserted user has id 1
    const auto got = r.getEventById(num(1));
    ASSERT_TRUE(got);
    EXPECT_EQ(got->location, "HQ");
    EXPECT_TRUE(got->isFlexible());
    EXPECT_EQ(r.getEventsByDate("2026-11-02").size(), 1u);
}
