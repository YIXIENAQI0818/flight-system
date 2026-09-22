#include "flight/flight_database.h"

#include <gtest/gtest.h>

#ifndef FLIGHT_DATA_DIR
#define FLIGHT_DATA_DIR "data"
#endif

using flight::DateTime;
using flight::Flight;
using flight::FlightDatabase;

namespace {

Flight mk(int id, int from, int to, const char* dep, const char* arr, int fare) {
    Flight f;
    f.id = id;
    f.flight_no = id;
    f.from_airport = from;
    f.to_airport = to;
    f.dep_time = DateTime::parse(dep, "%m/%d/%Y %H:%M");
    f.arr_time = DateTime::parse(arr, "%m/%d/%Y %H:%M");
    f.fare = fare;
    f.date = DateTime(f.dep_time.year, f.dep_time.month, f.dep_time.day);
    return f;
}

} // namespace

TEST(FlightDatabase, LoadRealData) {
    FlightDatabase db;
    db.load(std::string(FLIGHT_DATA_DIR) + "/flights.csv");
    EXPECT_EQ(db.size(), 2346u);
    EXPECT_GT(db.active_flights().size(), 0u);
}

TEST(FlightDatabase, Stats) {
    FlightDatabase db;
    db.load(std::string(FLIGHT_DATA_DIR) + "/flights.csv");
    const auto s = db.stats();
    ASSERT_TRUE(s.cheapest.has_value());
    ASSERT_TRUE(s.most_expensive.has_value());
    EXPECT_LE(s.cheapest->fare, s.most_expensive->fare);
    EXPECT_LE(s.earliest_departure->dep_time, s.latest_departure->dep_time);
    EXPECT_LE(s.shortest_duration->duration_minutes(),
              s.longest_duration->duration_minutes());
}

TEST(FlightDatabase, DirectFlights) {
    FlightDatabase db;
    db.load(std::string(FLIGHT_DATA_DIR) + "/flights.csv");
    const auto direct = db.direct_flights(48, 50);
    EXPECT_GT(direct.size(), 0u);
    for (const auto& f : direct) {
        EXPECT_EQ(f.from_airport, 48);
        EXPECT_EQ(f.to_airport, 50);
    }
}

TEST(FlightDatabase, BestFlightsCheapest) {
    FlightDatabase db;
    db.load(std::string(FLIGHT_DATA_DIR) + "/flights.csv");
    const auto direct = db.direct_flights(48, 50);
    const auto best = db.best_flights(48, 50);
    ASSERT_GT(best.cheapest.size(), 0u);
    int min_fare = direct.front().fare;
    for (const auto& f : direct) {
        min_fare = std::min(min_fare, f.fare);
    }
    EXPECT_EQ(best.cheapest.front().fare, min_fare);
}

TEST(FlightDatabase, AddFindUpdateRemove) {
    FlightDatabase db;
    db.load(std::string(FLIGHT_DATA_DIR) + "/flights.csv");
    const size_t before = db.size();

    db.add(mk(99999, 1, 2, "5/5/2017 08:00", "5/5/2017 10:00", 100));
    EXPECT_EQ(db.size(), before + 1);
    EXPECT_NE(db.find_by_id(99999), nullptr);

    db.update(mk(99999, 2, 3, "5/5/2017 11:00", "5/5/2017 13:00", 200));
    EXPECT_EQ(db.find_by_id(99999)->fare, 200);

    db.remove(99999);
    EXPECT_EQ(db.size(), before);
    EXPECT_EQ(db.find_by_id(99999), nullptr);
}

TEST(FlightDatabase, AddDuplicateThrows) {
    FlightDatabase db;
    db.load(std::string(FLIGHT_DATA_DIR) + "/flights.csv");
    EXPECT_THROW(db.add(mk(1, 1, 2, "5/5/2017 08:00", "5/5/2017 10:00", 100)),
                 std::invalid_argument);
}

TEST(FlightDatabase, RemoveNonexistentThrows) {
    FlightDatabase db;
    db.load(std::string(FLIGHT_DATA_DIR) + "/flights.csv");
    EXPECT_THROW(db.remove(123456789), std::invalid_argument);
}

TEST(FlightDatabase, SuspendAndResumeAirport) {
    FlightDatabase db;
    db.load(std::string(FLIGHT_DATA_DIR) + "/flights.csv");
    const auto before = db.direct_flights(48, 50);
    ASSERT_GT(before.size(), 0u);

    db.suspend_airport(48);
    EXPECT_TRUE(db.is_airport_suspended(48));
    EXPECT_EQ(db.direct_flights(48, 50).size(), 0u);

    db.resume_airport(48);
    EXPECT_FALSE(db.is_airport_suspended(48));
    EXPECT_EQ(db.direct_flights(48, 50).size(), before.size());
}
