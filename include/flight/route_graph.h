#ifndef FLIGHT_ROUTE_GRAPH_H
#define FLIGHT_ROUTE_GRAPH_H

#include "flight/flight.h"

#include <unordered_map>
#include <vector>

namespace flight {

// 基于"航班"的换乘图（flight connection graph）：
// 节点 = 一条活跃航班，有向边 i->j 表示"乘坐航班 i 后可在机场换乘航班 j"，
// 换乘条件：_flights[i].to_airport == _flights[j].from_airport 且
// _flights[j].dep_time >= _flights[i].arr_time + min_connection_minutes。
//
// 由于时间严格递增，该图为有向无环图（DAG），可做拓扑排序 / DP。
class FlightGraph {
public:
    // 用活跃航班构建换乘图。min_connection_minutes 为最小换乘间隔（分钟）。
    void build(const std::vector<Flight>& active, int min_connection_minutes = 30);

    const std::vector<Flight>& flights() const { return _flights; }
    const std::vector<std::vector<int>>& adjacency() const { return _adj; }   // i -> 可换乘的 j
    const std::vector<std::vector<int>>& reverse_adjacency() const { return _radj; }

    // 按起飞机场 / 到达机场分组的航班下标。
    const std::unordered_map<int, std::vector<int>>& by_from() const { return _by_from; }
    const std::unordered_map<int, std::vector<int>>& by_to() const { return _by_to; }

    // 航班 id -> 节点下标；未找到返回 -1。
    int index_of(int flight_id) const;
    int min_connection_minutes() const { return _min_connection; }

    // 从航班 i 换乘到航班 j 的等待分钟数（j.dep - i.arr）。
    int wait_minutes(int i, int j) const;

private:
    std::vector<Flight> _flights;
    std::vector<std::vector<int>> _adj;
    std::vector<std::vector<int>> _radj;
    std::unordered_map<int, std::vector<int>> _by_from;
    std::unordered_map<int, std::vector<int>> _by_to;
    std::unordered_map<int, int> _id_to_index;
    int _min_connection = 30;
};

} // namespace flight

#endif // FLIGHT_ROUTE_GRAPH_H
