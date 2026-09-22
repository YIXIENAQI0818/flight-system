#ifndef FLIGHT_FLIGHT_H
#define FLIGHT_FLIGHT_H

#include "flight/date_time.h"

namespace flight {

// 一条航班记录，对应 data/flights.csv 的一行。
// 字段顺序与 CSV 列一致：Flight ID / Departure date / Intl-Dome / Flight NO. /
// Departure airport / Arrival airport / Departure Time / Arrival Time /
// Airplane ID / Airplane Model / Air fares。
struct Flight {
    int id = 0;                 // 航班 ID
    DateTime date;              // 起飞日期
    bool is_international = false; // Intl / Dome
    int flight_no = 0;          // 航班编号
    int from_airport = 0;       // 起飞机场 ID
    int to_airport = 0;         // 到达机场 ID
    DateTime dep_time;          // 起飞时间
    DateTime arr_time;          // 到达时间
    int airplane_id = 0;        // 飞机 ID
    int airplane_model = 0;     // 飞机机型
    int fare = 0;               // 基础票价

    // 飞行时长（分钟）。
    int duration_minutes() const { return minutes_between(dep_time, arr_time); }
};

} // namespace flight

#endif // FLIGHT_FLIGHT_H
