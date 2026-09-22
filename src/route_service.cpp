#include "flight/route_service.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>

namespace flight {

namespace {

constexpr long long kInf = std::numeric_limits<long long>::max() / 4;

std::vector<int> by_from_safe(const FlightGraph& g, int airport) {
    const auto it = g.by_from().find(airport);
    return it == g.by_from().end() ? std::vector<int>{} : it->second;
}

} // namespace

Criterion parse_criterion(const std::string& s) {
    if (s == "duration") {
        return Criterion::Duration;
    }
    if (s == "fare") {
        return Criterion::Fare;
    }
    throw std::invalid_argument("未知评价维度: '" + s + "'（应为 duration 或 fare）");
}

std::vector<int> RouteService::topological_order(const FlightGraph& g) {
    std::vector<int> order(g.flights().size());
    for (size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<int>(i);
    }
    std::stable_sort(order.begin(), order.end(), [&g](int a, int b) {
        return g.flights()[a].dep_time.total_minutes() <
               g.flights()[b].dep_time.total_minutes();
    });
    return order;
}

std::vector<std::vector<int>> RouteService::connectivity(
    int from, int to, int max_transfers,
    const std::optional<TimeWindow>& dep_window,
    const std::optional<TimeWindow>& arr_window) const {
    FlightGraph g;
    g.build(db_.active_flights(), kMinConnectionMinutes);

    std::vector<std::vector<int>> result;
    std::vector<int> path;

    std::function<void(int, int)> dfs = [&](int node, int remaining) {
        const Flight& f = g.flights()[node];
        if (f.to_airport == to) {
            if (!arr_window || arr_window->contains(f.arr_time)) {
                result.push_back(path);
            }
            return; // 已到达，无需继续换乘
        }
        if (remaining <= 0) {
            return;
        }
        for (int j : g.adjacency()[node]) {
            path.push_back(g.flights()[j].id);
            dfs(j, remaining - 1);
            path.pop_back();
        }
    };

    for (int i : by_from_safe(g, from)) {
        const Flight& f = g.flights()[i];
        if (dep_window && !dep_window->contains(f.dep_time)) {
            continue;
        }
        path.assign(1, f.id);
        dfs(i, max_transfers);
    }
    return result;
}

std::vector<std::vector<int>> RouteService::optimal_routes(int from, int to,
                                                           Criterion criterion) const {
    FlightGraph g;
    g.build(db_.active_flights(), kMinConnectionMinutes);
    const auto& flights = g.flights();
    const size_t n = flights.size();

    const auto source_cost = [&](int i) -> long long {
        return criterion == Criterion::Fare ? flights[i].fare
                                            : flights[i].duration_minutes();
    };
    const auto edge_cost = [&](int i, int j) -> long long {
        if (criterion == Criterion::Fare) {
            return flights[j].fare;
        }
        return flights[j].duration_minutes() + g.wait_minutes(i, j);
    };

    const std::vector<int> order = topological_order(g);
    std::vector<long long> dist(n, kInf);
    for (int i : by_from_safe(g, from)) {
        dist[i] = source_cost(i);
    }
    for (int i : order) {
        if (dist[i] >= kInf) {
            continue;
        }
        for (int j : g.adjacency()[i]) {
            dist[j] = std::min(dist[j], dist[i] + edge_cost(i, j));
        }
    }

    long long optimal = kInf;
    const auto target_nodes = g.by_to().count(to) ? g.by_to().at(to) : std::vector<int>{};
    for (int i : target_nodes) {
        optimal = std::min(optimal, dist[i]);
    }

    std::vector<std::vector<int>> result;
    if (optimal >= kInf) {
        return result;
    }

    // 反向可达性：哪些节点能（沿换乘边）到达某个 target 节点。
    std::vector<bool> reach_target(n, false);
    std::vector<int> stack;
    for (int t : target_nodes) {
        reach_target[t] = true;
        stack.push_back(t);
    }
    while (!stack.empty()) {
        int cur = stack.back();
        stack.pop_back();
        for (int p : g.reverse_adjacency()[cur]) {
            if (!reach_target[p]) {
                reach_target[p] = true;
                stack.push_back(p);
            }
        }
    }

    // 沿"最短路上的边"DFS，枚举全部最优方案。
    std::vector<int> path;
    std::function<void(int)> dfs = [&](int i) {
        if (flights[i].to_airport == to && dist[i] == optimal) {
            result.push_back(path);
            return;
        }
        for (int j : g.adjacency()[i]) {
            if (reach_target[j] && dist[j] == dist[i] + edge_cost(i, j)) {
                path.push_back(flights[j].id);
                dfs(j);
                path.pop_back();
            }
        }
    };

    for (int i : by_from_safe(g, from)) {
        if (dist[i] >= kInf || !reach_target[i]) {
            continue;
        }
        path.assign(1, flights[i].id);
        dfs(i);
    }
    return result;
}

RouteService::MaxFlightsResult RouteService::max_flights(int start_flight_id) const {
    MaxFlightsResult res;
    FlightGraph g;
    g.build(db_.active_flights(), kMinConnectionMinutes);
    const int start = g.index_of(start_flight_id);
    if (start < 0) {
        // 起始航班不存在或已被暂停。
        return res;
    }

    const size_t n = g.flights().size();
    std::vector<int> order = topological_order(g);
    std::vector<int> dist(n, -1); // 最长路径的航班数，-1 表示不可达
    dist[start] = 1;
    for (int i : order) {
        if (dist[i] < 0) {
            continue;
        }
        for (int j : g.adjacency()[i]) {
            dist[j] = std::max(dist[j], dist[i] + 1);
        }
    }

    res.max_count = *std::max_element(dist.begin(), dist.end());

    // 枚举全部最长路线。
    std::vector<int> path;
    std::function<void(int)> dfs = [&](int i) {
        if (dist[i] == res.max_count) {
            res.routes.push_back(path);
            return;
        }
        for (int j : g.adjacency()[i]) {
            if (dist[j] == dist[i] + 1) {
                path.push_back(g.flights()[j].id);
                dfs(j);
                path.pop_back();
            }
        }
    };
    path.assign(1, start_flight_id);
    dfs(start);
    return res;
}

} // namespace flight
