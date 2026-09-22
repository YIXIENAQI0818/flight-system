#ifndef FLIGHT_ROUTE_GRAPH_H
#define FLIGHT_ROUTE_GRAPH_H

#include "flight/flight.h"

#include <unordered_map>
#include <vector>

namespace flight {

// 基于"航班"的换乘图（flight connection graph）：
// 节点 = 一条活跃航班，有向边 i->j 表示"乘坐航班 i 后可在机场换乘航班 j"，
// 换乘条件：flights_[i].to_airport == flights_[j].from_airport 且
// flights_[j].dep_time >= flights_[i].arr_time + min_connection_minutes。
//
// 由于时间严格递增，该图为有向无环图（DAG），可做拓扑排序 / DP。
class FlightGraph {
public:
    // 用活跃航班构建换乘图。min_connection_minutes 为最小换乘间隔（分钟）。
    void build(const std::vector<Flight>& active, int min_connection_minutes = 30);

    const std::vector<Flight>& flights() const { return flights_; }
    const std::vector<std::vector<int>>& adjacency() const { return adj_; }   // i -> 可换乘的 j
    const std::vector<std::vector<int>>& reverse_adjacency() const { return radj_; }

    // 按起飞机场 / 到达机场分组的航班下标。
    const std::unordered_map<int, std::vector<int>>& by_from() const { return by_from_; }
    const std::unordered_map<int, std::vector<int>>& by_to() const { return by_to_; }

    // 航班 id -> 节点下标；未找到返回 -1。
    int index_of(int flight_id) const;
    int min_connection_minutes() const { return min_connection_; }

    // 从航班 i 换乘到航班 j 的等待分钟数（j.dep - i.arr）。
    int wait_minutes(int i, int j) const;

private:
    std::vector<Flight> flights_;
    std::vector<std::vector<int>> adj_;
    std::vector<std::vector<int>> radj_;
    std::unordered_map<int, std::vector<int>> by_from_;
    std::unordered_map<int, std::vector<int>> by_to_;
    std::unordered_map<int, int> id_to_index_;
    int min_connection_ = 30;
};

} // namespace flight

#endif // FLIGHT_ROUTE_GRAPH_H
