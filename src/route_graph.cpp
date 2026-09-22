#include "flight/route_graph.h"

namespace flight {

void FlightGraph::build(const std::vector<Flight>& active, int min_connection_minutes) {
    flights_ = active;
    min_connection_ = min_connection_minutes;

    const size_t n = flights_.size();
    adj_.assign(n, {});
    radj_.assign(n, {});
    by_from_.clear();
    by_to_.clear();
    id_to_index_.clear();

    for (size_t i = 0; i < n; ++i) {
        by_from_[flights_[i].from_airport].push_back(static_cast<int>(i));
        by_to_[flights_[i].to_airport].push_back(static_cast<int>(i));
        id_to_index_[flights_[i].id] = static_cast<int>(i);
    }

    for (size_t i = 0; i < n; ++i) {
        const auto it = by_from_.find(flights_[i].to_airport);
        if (it == by_from_.end()) {
            continue;
        }
        const long long earliest_dep =
            flights_[i].arr_time.total_minutes() + min_connection_;
        for (int j : it->second) {
            if (flights_[j].dep_time.total_minutes() >= earliest_dep) {
                adj_[i].push_back(j);
                radj_[j].push_back(static_cast<int>(i));
            }
        }
    }
}

int FlightGraph::index_of(int flight_id) const {
    const auto it = id_to_index_.find(flight_id);
    return it == id_to_index_.end() ? -1 : it->second;
}

int FlightGraph::wait_minutes(int i, int j) const {
    return static_cast<int>(flights_[j].dep_time.total_minutes() -
                            flights_[i].arr_time.total_minutes());
}

} // namespace flight
