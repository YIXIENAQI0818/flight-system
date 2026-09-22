#include "flight/airport_service.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>

#ifndef FLIGHT_DATA_DIR
#define FLIGHT_DATA_DIR "data"
#endif

using flight::AirportService;
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

TEST(AirportService, LoadRealAirports) {
    AirportService svc;
    svc.load(std::string(FLIGHT_DATA_DIR) + "/airports.csv");
    EXPECT_EQ(svc.airports().size(), 79u);
}

TEST(AirportService, SearchByName) {
    AirportService svc;
    svc.load(std::string(FLIGHT_DATA_DIR) + "/airports.csv");
    const auto results = svc.search_by_name("Beijing", 5);
    ASSERT_FALSE(results.empty());
    EXPECT_NE(results[0].airport.full_name().find("Beijing"), std::string::npos);
    // 结果应按相似度降序。
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_LE(results[i].score, results[i - 1].score);
    }
}

TEST(AirportService, BusiestAirportUnique) {
    FlightDatabase db;
    db.add(mk(1, 1, 2, "5/5/2017 08:00", "5/5/2017 09:00", 100));
    db.add(mk(2, 1, 2, "5/5/2017 09:00", "5/5/2017 10:00", 100));
    db.add(mk(3, 1, 3, "5/5/2017 10:00", "5/5/2017 11:00", 100));

    AirportService svc;
    const auto traffic = svc.busiest_airports(db);
    ASSERT_EQ(traffic.size(), 1u);
    EXPECT_EQ(traffic[0].airport_id, 1);
    EXPECT_EQ(traffic[0].departures, 3);
    EXPECT_EQ(traffic[0].arrivals, 0);
    EXPECT_EQ(traffic[0].total(), 3);
}

TEST(AirportService, RecommendSameProvince) {
    const std::string path = "/tmp/fs_test_airports.csv";
    {
        std::ofstream f(path);
        f << "ID,Country,Province,Name\n"
          << "1,China,Beijing,Capital\n"
          << "2,China,Beijing,Daxing\n"
          << "3,China,Shanghai,Pudong\n"
          << "4,China,Shanghai,Hongqiao\n";
    }

    AirportService svc;
    svc.load(path);

    FlightDatabase db;
    // 无 1->3 直达，但有同省 2->4 直达（北京大兴 -> 上海虹桥）。
    db.add(mk(10, 2, 4, "5/5/2017 08:00", "5/5/2017 10:00", 300));
    db.add(mk(11, 2, 3, "5/5/2017 09:00", "5/5/2017 11:00", 400));

    const auto rec = svc.recommend_same_province(db, 1, 3);
    ASSERT_EQ(rec.size(), 2u);
    for (const auto& f : rec) {
        EXPECT_TRUE(f.from_airport == 1 || f.from_airport == 2);
        EXPECT_TRUE(f.to_airport == 3 || f.to_airport == 4);
    }
}
