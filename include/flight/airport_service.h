#ifndef FLIGHT_AIRPORT_SERVICE_H
#define FLIGHT_AIRPORT_SERVICE_H

#include "flight/airport.h"
#include "flight/flight_database.h"
#include "flight/route_service.h" // TimeWindow

#include <string>
#include <unordered_map>
#include <vector>

namespace flight {

// 机场服务：机场数据加载、名称近似搜索、同省推荐、繁忙机场统计（实验四/五）。
class AirportService {
public:
    // 带相似度分数的机场结果。
    struct ScoredAirport {
        Airport airport;
        double score = 0.0;
    };

    // 单个机场的起降统计。
    struct AirportTraffic {
        int airport_id = 0;
        int departures = 0;
        int arrivals = 0;
        int total() const { return departures + arrivals; }
    };

    void load(const std::string& path);

    const std::vector<Airport>& airports() const { return _airports; }
    const Airport* find_by_id(int id) const;

    // 实验四1：机场名近似搜索，返回相似度降序的前 k 个。
    std::vector<ScoredAirport> search_by_name(const std::string& query, size_t k = 5) const;

    // 实验四2：同省机场推荐。返回"起飞机场同省机场 -> 到达机场同省机场"的直达航班。
    // 调用方通常在无直达时使用。
    std::vector<Flight> recommend_same_province(const FlightDatabase& db,
                                                int from, int to) const;

    // 实验五：最繁忙机场。dep_window 过滤起飞时间，arr_window 过滤降落时间。
    // 返回全部并列最繁忙的机场。
    std::vector<AirportTraffic> busiest_airports(
        const FlightDatabase& db,
        const std::optional<TimeWindow>& dep_window = std::nullopt,
        const std::optional<TimeWindow>& arr_window = std::nullopt) const;

    // 机场名与查询词之间的相似度（字符集包含度 + 最长公共子串 + 精确匹配加成）。
    static double similarity(const std::string& query, const Airport& airport);

private:
    std::vector<Airport> _airports;
    std::unordered_map<int, size_t> _id_to_index;
    std::unordered_map<std::string, std::vector<int>> _by_province; // 省份 -> 机场 id
};

} // namespace flight

#endif // FLIGHT_AIRPORT_SERVICE_H
