#include "flight/airport_service.h"

#include "flight/csv_reader.h"

#include <algorithm>
#include <cctype>
#include <array>
#include <stdexcept>

namespace flight {

namespace {

int parse_int_field(const std::string& s) {
    return std::stoi(s);
}

// 最长公共子串长度（不区分大小写）。
int longest_common_substring(const std::string& a, const std::string& b) {
    const size_t n = a.size();
    const size_t m = b.size();
    std::vector<int> prev(m + 1, 0), cur(m + 1, 0);
    int best = 0;
    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (std::tolower(static_cast<unsigned char>(a[i - 1])) ==
                std::tolower(static_cast<unsigned char>(b[j - 1]))) {
                cur[j] = prev[j - 1] + 1;
                best = std::max(best, cur[j]);
            } else {
                cur[j] = 0;
            }
        }
        std::swap(prev, cur);
        std::fill(cur.begin(), cur.end(), 0);
    }
    return best;
}

} // namespace

void AirportService::load(const std::string& path) {
    const auto table = CsvReader::read(path, /*skip_header=*/true);
    _airports.clear();
    _by_province.clear();
    _id_to_index.clear();
    _airports.reserve(table.size());
    for (const auto& row : table) {
        if (row.size() < 4) {
            throw std::runtime_error("机场记录字段不足（需要 4 列）");
        }
        Airport a;
        a.id = parse_int_field(row[0]);
        a.country = row[1];
        a.province = row[2];
        a.name = row[3];
        _id_to_index[a.id] = _airports.size();
        _by_province[a.province].push_back(a.id); // 必须在 move 之前读取 a 的字段
        _airports.push_back(std::move(a));
    }
}

const Airport* AirportService::find_by_id(int id) const {
    const auto it = _id_to_index.find(id);
    return it == _id_to_index.end() ? nullptr : &_airports[it->second];
}

double AirportService::similarity(const std::string& query, const Airport& airport) {
    double result = 0.0;
    if (airport.country == query || airport.province == query || airport.name == query) {
        result += 1.0;
    }

    std::array<int, 26> query_set{};
    std::array<int, 26> name_set{};
    for (char ch : query) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            query_set[std::tolower(static_cast<unsigned char>(ch)) - 'a'] = 1;
        }
    }
    for (char ch : airport.name) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            name_set[std::tolower(static_cast<unsigned char>(ch)) - 'a'] = 1;
        }
    }
    int inter = 0;
    int qcount = 0;
    for (int i = 0; i < 26; ++i) {
        if (query_set[i] && name_set[i]) {
            ++inter;
        }
        if (query_set[i]) {
            ++qcount;
        }
    }

    const double containment = qcount > 0 ? static_cast<double>(inter) / qcount : 0.0;
    const double continuous = query.empty()
                                  ? 0.0
                                  : static_cast<double>(longest_common_substring(query, airport.name)) /
                                        query.size();

    const double w1 = 0.3, w2 = 0.7; // 包含性 / 连续匹配权重
    result += w1 * containment + w2 * continuous;
    return result;
}

std::vector<AirportService::ScoredAirport>
AirportService::search_by_name(const std::string& query, size_t k) const {
    std::vector<ScoredAirport> results;
    results.reserve(_airports.size());
    for (const auto& a : _airports) {
        results.push_back({a, similarity(query, a)});
    }
    std::sort(results.begin(), results.end(), [](const ScoredAirport& x, const ScoredAirport& y) {
        return x.score > y.score;
    });
    if (results.size() > k) {
        results.resize(k);
    }
    return results;
}

std::vector<Flight> AirportService::recommend_same_province(const FlightDatabase& db,
                                                            int from, int to) const {
    const Airport* dep = find_by_id(from);
    const Airport* arr = find_by_id(to);
    if (dep == nullptr || arr == nullptr) {
        return {};
    }
    std::vector<int> dep_candidates = _by_province.count(dep->province)
                                          ? _by_province.at(dep->province)
                                          : std::vector<int>{};
    std::vector<int> arr_candidates = _by_province.count(arr->province)
                                          ? _by_province.at(arr->province)
                                          : std::vector<int>{};

    std::unordered_map<int, bool> dep_set, arr_set;
    for (int id : dep_candidates) {
        dep_set[id] = true;
    }
    for (int id : arr_candidates) {
        arr_set[id] = true;
    }

    std::vector<Flight> result;
    for (const auto& f : db.active_flights()) {
        if (dep_set.count(f.from_airport) && arr_set.count(f.to_airport)) {
            result.push_back(f);
        }
    }
    return result;
}

std::vector<AirportService::AirportTraffic> AirportService::busiest_airports(
    const FlightDatabase& db, const std::optional<TimeWindow>& dep_window,
    const std::optional<TimeWindow>& arr_window) const {
    std::unordered_map<int, AirportTraffic> traffic;
    for (const auto& f : db.active_flights()) {
        if (!dep_window || dep_window->contains(f.dep_time)) {
            traffic[f.from_airport].departures += 1;
            traffic[f.from_airport].airport_id = f.from_airport;
        }
        if (!arr_window || arr_window->contains(f.arr_time)) {
            traffic[f.to_airport].arrivals += 1;
            traffic[f.to_airport].airport_id = f.to_airport;
        }
    }

    int max_total = 0;
    for (const auto& kv : traffic) {
        max_total = std::max(max_total, kv.second.total());
    }

    std::vector<AirportTraffic> result;
    for (const auto& kv : traffic) {
        if (kv.second.total() == max_total) {
            result.push_back(kv.second);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const AirportTraffic& a, const AirportTraffic& b) {
                  return a.airport_id < b.airport_id;
              });
    return result;
}

} // namespace flight
