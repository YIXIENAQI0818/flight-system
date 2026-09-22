#ifndef FLIGHT_AIRPORT_H
#define FLIGHT_AIRPORT_H

#include <string>

namespace flight {

// 机场信息，对应 data/airports.csv 的一行：ID / Country / Province / Name。
struct Airport {
    int id = 0;
    std::string country;
    std::string province;
    std::string name;

    // 完整名称，形如 "China/Beijing/Beijing Capital"。
    std::string full_name() const { return country + "/" + province + "/" + name; }
};

} // namespace flight

#endif // FLIGHT_AIRPORT_H
