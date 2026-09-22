#include "flight/route_service.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <stdexcept>

using flight::DateTime;
using flight::Flight;
using flight::FlightDatabase;
using flight::RouteService;

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

// 构造一个小型换乘场景：
//   f1: 1->2  08:00-10:00  fare 100
//   f2: 2->3  10:30-12:00  fare 200
//   f3: 1->3  09:00-11:00  fare 500   (直达)
//   f4: 2->3  11:00-13:00  fare 150
//   f5: 3->4  12:30-14:00  fare 50
FlightDatabase make_db() {
    FlightDatabase db;
    db.add(mk(1, 1, 2, "5/5/2017 08:00", "5/5/2017 10:00", 100));
    db.add(mk(2, 2, 3, "5/5/2017 10:30", "5/5/2017 12:00", 200));
    db.add(mk(3, 1, 3, "5/5/2017 09:00", "5/5/2017 11:00", 500));
    db.add(mk(4, 2, 3, "5/5/2017 11:00", "5/5/2017 13:00", 150));
    db.add(mk(5, 3, 4, "5/5/2017 12:30", "5/5/2017 14:00", 50));
    return db;
}

bool contains(const std::vector<std::vector<int>>& routes, const std::vector<int>& r) {
    return std::find(routes.begin(), routes.end(), r) != routes.end();
}

} // namespace

TEST(RouteService, ConnectivityDirectAndOneTransfer) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    const auto routes = svc.connectivity(1, 3, 1);
    ASSERT_EQ(routes.size(), 3u);
    EXPECT_TRUE(contains(routes, {3}));
    EXPECT_TRUE(contains(routes, {1, 2}));
    EXPECT_TRUE(contains(routes, {1, 4}));
}

TEST(RouteService, ConnectivityOneTransferViaIntermediate) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    // 1 -> 3(直达 f3) -> 4(f5) 是一次中转。
    const auto routes = svc.connectivity(1, 4, 1);
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_TRUE(contains(routes, {3, 5}));
}

TEST(RouteService, ConnectivityTwoTransfers) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    // 1 -> 3 -> 4 的一次中转，以及 1 -> 2 -> 3 -> 4 的两次中转。
    const auto routes = svc.connectivity(1, 4, 2);
    ASSERT_EQ(routes.size(), 2u);
    EXPECT_TRUE(contains(routes, {1, 2, 5}));
    EXPECT_TRUE(contains(routes, {3, 5}));
}

TEST(RouteService, OptimalDuration) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    const auto routes = svc.optimal_routes(1, 3, flight::Criterion::Duration);
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes[0], (std::vector<int>{3}));
}

TEST(RouteService, OptimalFare) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    const auto routes = svc.optimal_routes(1, 3, flight::Criterion::Fare);
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes[0], (std::vector<int>{1, 4}));
}

TEST(RouteService, MaxFlights) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    const auto res = svc.max_flights(1);
    EXPECT_EQ(res.max_count, 3);
    ASSERT_EQ(res.routes.size(), 1u);
    EXPECT_EQ(res.routes[0], (std::vector<int>{1, 2, 5}));
}

TEST(RouteService, MaxFlightsUnknownStart) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    const auto res = svc.max_flights(9999);
    EXPECT_EQ(res.max_count, 0);
    EXPECT_TRUE(res.routes.empty());
}

TEST(RouteService, SuspendedAirportAffectsConnectivity) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    db.suspend_airport(2);
    const auto routes = svc.connectivity(1, 3, 1);
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes[0], (std::vector<int>{3}));
}

TEST(RouteService, ParseCriterion) {
    EXPECT_EQ(flight::parse_criterion("duration"), flight::Criterion::Duration);
    EXPECT_EQ(flight::parse_criterion("fare"), flight::Criterion::Fare);
    EXPECT_THROW(flight::parse_criterion("xxx"), std::invalid_argument);
}

TEST(RouteService, ConnectivityDepWindow) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    // 首段起飞 >= 9:00,排除 f1(08:00 起飞)
    const flight::TimeWindow dep_win{DateTime(2017, 5, 5, 9, 0), std::nullopt};
    const auto routes = svc.connectivity(1, 3, 1, dep_win, std::nullopt);
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes[0], (std::vector<int>{3}));
}

TEST(RouteService, ConnectivityArrWindow) {
    FlightDatabase db = make_db();
    RouteService svc(db);
    // 末段到达 <= 12:00,排除 f4(13:00 到达)
    const flight::TimeWindow arr_win{std::nullopt, DateTime(2017, 5, 5, 12, 0)};
    const auto routes = svc.connectivity(1, 3, 1, std::nullopt, arr_win);
    ASSERT_EQ(routes.size(), 2u);
    EXPECT_TRUE(contains(routes, {3}));
    EXPECT_TRUE(contains(routes, {1, 2}));
}

TEST(RouteService, OptimalFareTie) {
    FlightDatabase db;
    db.add(mk(1, 1, 2, "5/5/2017 08:00", "5/5/2017 10:00", 100));
    db.add(mk(4, 2, 3, "5/5/2017 11:00", "5/5/2017 13:00", 150));
    db.add(mk(3, 1, 3, "5/5/2017 09:00", "5/5/2017 11:00", 250));  // 直飞 250
    RouteService svc(db);
    // 两条路线票价相等:直飞 f3=250,换乘 f1->f4=100+150=250
    const auto routes = svc.optimal_routes(1, 3, flight::Criterion::Fare);
    ASSERT_EQ(routes.size(), 2u);
    EXPECT_TRUE(contains(routes, {3}));
    EXPECT_TRUE(contains(routes, {1, 4}));
}
