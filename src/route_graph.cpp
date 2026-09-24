#include "flight/route_graph.h"

namespace flight {

void FlightGraph::build(const std::vector<Flight>& active, int min_connection_minutes) {
    _flights = active;
    _min_connection = min_connection_minutes;

    const size_t n = _flights.size();
    _adj.assign(n, {});
    _radj.assign(n, {});
    _by_from.clear();
    _by_to.clear();
    _id_to_index.clear();

    for (size_t i = 0; i < n; ++i) {
        _by_from[_flights[i].from_airport].push_back(static_cast<int>(i));
        _by_to[_flights[i].to_airport].push_back(static_cast<int>(i));
        _id_to_index[_flights[i].id] = static_cast<int>(i);
    }

    for (size_t i = 0; i < n; ++i) {
        const auto it = _by_from.find(_flights[i].to_airport);
        if (it == _by_from.end()) {
            continue;
        }
        const long long earliest_dep =
            _flights[i].arr_time.total_minutes() + _min_connection;
        for (int j : it->second) {
            if (_flights[j].dep_time.total_minutes() >= earliest_dep) {
                _adj[i].push_back(j);
                _radj[j].push_back(static_cast<int>(i));
            }
        }
    }
}

int FlightGraph::index_of(int flight_id) const {
    const auto it = _id_to_index.find(flight_id);
    return it == _id_to_index.end() ? -1 : it->second;
}

int FlightGraph::wait_minutes(int i, int j) const {
    return static_cast<int>(_flights[j].dep_time.total_minutes() -
                            _flights[i].arr_time.total_minutes());
}

} // namespace flight
