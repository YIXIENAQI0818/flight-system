#ifndef FLIGHT_ROUTE_SERVICE_H
#define FLIGHT_ROUTE_SERVICE_H

#include "flight/flight_database.h"
#include "flight/route_graph.h"

#include <optional>
#include <string>
#include <vector>

namespace flight {

// 全局最小换乘间隔（分钟）。与实验三"换乘需留出 30 分钟"一致，对实验六同样适用。
constexpr int kMinConnectionMinutes = 30;

// 时间区间（含端点）。
struct TimeWindow {
    std::optional<DateTime> start;
    std::optional<DateTime> end;

    bool contains(const DateTime& t) const {
        return (!start || *start <= t) && (!end || t <= *end);
    }
};

// 最优方案的评价维度。
enum class Criterion { Duration, Fare };

// 解析 "duration" / "fare" 字符串；非法值抛 std::invalid_argument。
Criterion parse_criterion(const std::string& s);

// 航线服务：基于 FlightDatabase 提供连通性、最优乘机方案、最多乘机等业务能力。
// 内部使用 FlightGraph（航班换乘图）做图算法。
class RouteService {
public:
    explicit RouteService(const FlightDatabase& db) : db_(db) {}

    // 实验六1：连通性。返回从 from 到 to 的所有可行乘机方案（航班 ID 序列）。
    // max_transfers 为中转次数上限（0=直飞，1=可一次中转）。
    // dep_window 约束首段起飞时间；arr_window 约束末段到达时间。
    std::vector<std::vector<int>> connectivity(
        int from, int to, int max_transfers = 1,
        const std::optional<TimeWindow>& dep_window = std::nullopt,
        const std::optional<TimeWindow>& arr_window = std::nullopt) const;

    // 实验六2：不限中转次数，按 criterion（duration/fare）求最优，返回全部最优方案。
    std::vector<std::vector<int>> optimal_routes(int from, int to, Criterion criterion) const;

    // 实验三3：最多乘机。给定起始航班 ID，返回最多可乘坐航班次数 + 全部最长路线。
    struct MaxFlightsResult {
        int max_count = 0;
        std::vector<std::vector<int>> routes; // 每条路线为航班 ID 序列（含起始航班）
    };
    MaxFlightsResult max_flights(int start_flight_id) const;

private:
    const FlightDatabase& db_;

    // 按起飞时间排序的节点下标（FlightGraph 的拓扑序）。
    static std::vector<int> topological_order(const FlightGraph& g);
};

} // namespace flight

#endif // FLIGHT_ROUTE_SERVICE_H
