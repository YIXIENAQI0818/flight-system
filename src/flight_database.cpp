#include "flight/flight_database.h"

#include "flight/csv_reader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace flight {

namespace {

constexpr const char* kDateFmt = "%m/%d/%Y";
constexpr const char* kTimeFmt = "%m/%d/%Y %H:%M";

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) {
        return "";
    }
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

int parse_int(const std::string& s, const std::string& what) {
    try {
        return std::stoi(trim(s));
    } catch (const std::exception&) {
        throw std::invalid_argument("字段 " + what + " 不是合法整数: '" + s + "'");
    }
}

} // namespace

Flight FlightDatabase::parse_row(const std::vector<std::string>& fields) {
    if (fields.size() < 11) {
        throw std::invalid_argument("航班记录字段不足（需要 11 列，实际 " +
                                    std::to_string(fields.size()) + " 列）");
    }
    Flight f;
    f.id = parse_int(fields[0], "Flight ID");
    f.date = DateTime::parse(trim(fields[1]), kDateFmt);
    f.is_international = (trim(fields[2]) == "Intl");
    f.flight_no = parse_int(fields[3], "Flight NO.");
    f.from_airport = parse_int(fields[4], "Departure airport");
    f.to_airport = parse_int(fields[5], "Arrival airport");
    f.dep_time = DateTime::parse(trim(fields[6]), kTimeFmt);
    f.arr_time = DateTime::parse(trim(fields[7]), kTimeFmt);
    f.airplane_id = parse_int(fields[8], "Airplane ID");
    f.airplane_model = parse_int(fields[9], "Airplane Model");
    f.fare = parse_int(fields[10], "Air fares");

    if (f.from_airport == f.to_airport) {
        throw std::invalid_argument("航班 " + std::to_string(f.id) + " 起降机场相同");
    }
    if (f.dep_time >= f.arr_time) {
        throw std::invalid_argument("航班 " + std::to_string(f.id) + " 起飞时间不早于到达时间");
    }
    if (f.fare < 0) {
        throw std::invalid_argument("航班 " + std::to_string(f.id) + " 票价为负");
    }
    return f;
}

void FlightDatabase::rebuild_index() {
    id_to_index_.clear();
    for (size_t i = 0; i < flights_.size(); ++i) {
        id_to_index_[flights_[i].id] = i;
    }
}

void FlightDatabase::load(const std::string& path) {
    const auto table = CsvReader::read(path, /*skip_header=*/true);
    flights_.clear();
    flights_.reserve(table.size());
    for (const auto& row : table) {
        flights_.push_back(parse_row(row));
    }
    rebuild_index();
    suspended_airports_.clear();
}

const Flight* FlightDatabase::find_by_id(int id) const {
    const auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return nullptr;
    }
    return &flights_[it->second];
}

bool FlightDatabase::is_airport_suspended(int airport_id) const {
    return suspended_airports_.count(airport_id) > 0;
}

bool FlightDatabase::is_flight_active(const Flight& f) const {
    return !is_airport_suspended(f.from_airport) && !is_airport_suspended(f.to_airport);
}

std::vector<Flight> FlightDatabase::active_flights() const {
    std::vector<Flight> result;
    result.reserve(flights_.size());
    for (const auto& f : flights_) {
        if (is_flight_active(f)) {
            result.push_back(f);
        }
    }
    return result;
}

void FlightDatabase::add(const Flight& f) {
    if (find_by_id(f.id) != nullptr) {
        throw std::invalid_argument("航班 ID " + std::to_string(f.id) + " 已存在");
    }
    if (f.from_airport == f.to_airport) {
        throw std::invalid_argument("航班 ID " + std::to_string(f.id) + " 起降机场相同");
    }
    if (f.dep_time >= f.arr_time) {
        throw std::invalid_argument("航班 ID " + std::to_string(f.id) + " 起飞时间不早于到达时间");
    }
    flights_.push_back(f);
    id_to_index_[f.id] = flights_.size() - 1;
}

void FlightDatabase::remove(int id) {
    const auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        throw std::invalid_argument("航班 ID " + std::to_string(id) + " 不存在，无法删除");
    }
    const size_t idx = it->second;
    flights_[idx] = flights_.back();
    flights_.pop_back();
    rebuild_index();
}

void FlightDatabase::update(const Flight& f) {
    const auto it = id_to_index_.find(f.id);
    if (it == id_to_index_.end()) {
        throw std::invalid_argument("航班 ID " + std::to_string(f.id) + " 不存在，无法修改");
    }
    if (f.from_airport == f.to_airport) {
        throw std::invalid_argument("航班 ID " + std::to_string(f.id) + " 起降机场相同");
    }
    if (f.dep_time >= f.arr_time) {
        throw std::invalid_argument("航班 ID " + std::to_string(f.id) + " 起飞时间不早于到达时间");
    }
    flights_[it->second] = f;
}

void FlightDatabase::suspend_airport(int airport_id) {
    suspended_airports_.insert(airport_id);
}

void FlightDatabase::resume_airport(int airport_id) {
    suspended_airports_.erase(airport_id);
}

FlightStats FlightDatabase::stats() const {
    FlightStats s;
    for (const auto& f : flights_) {
        if (!is_flight_active(f)) {
            continue;
        }
        if (!s.earliest_departure || f.dep_time < s.earliest_departure->dep_time) {
            s.earliest_departure = f;
        }
        if (!s.latest_departure || f.dep_time > s.latest_departure->dep_time) {
            s.latest_departure = f;
        }
        if (!s.shortest_duration || f.duration_minutes() < s.shortest_duration->duration_minutes()) {
            s.shortest_duration = f;
        }
        if (!s.longest_duration || f.duration_minutes() > s.longest_duration->duration_minutes()) {
            s.longest_duration = f;
        }
        if (!s.cheapest || f.fare < s.cheapest->fare) {
            s.cheapest = f;
        }
        if (!s.most_expensive || f.fare > s.most_expensive->fare) {
            s.most_expensive = f;
        }
    }
    return s;
}

std::vector<Flight> FlightDatabase::direct_flights(int from, int to) const {
    std::vector<Flight> result;
    for (const auto& f : flights_) {
        if (is_flight_active(f) && f.from_airport == from && f.to_airport == to) {
            result.push_back(f);
        }
    }
    return result;
}

BestFlights FlightDatabase::best_flights(int from, int to) const {
    BestFlights best;
    const auto direct = direct_flights(from, to);
    if (direct.empty()) {
        return best;
    }

    int min_fare = direct.front().fare;
    int min_dur = direct.front().duration_minutes();
    for (const auto& f : direct) {
        min_fare = std::min(min_fare, f.fare);
        min_dur = std::min(min_dur, f.duration_minutes());
    }
    for (const auto& f : direct) {
        if (f.fare == min_fare) {
            best.cheapest.push_back(f);
        }
        if (f.duration_minutes() == min_dur) {
            best.shortest_duration.push_back(f);
        }
        if (is_next_day(f.dep_time, f.arr_time)) {
            best.plus_one_day.push_back(f);
        }
    }
    return best;
}

void FlightDatabase::batch_apply(const std::string& ops_path) {
    std::ifstream file(ops_path);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开批量操作文件: " + ops_path);
    }

    std::string line;
    int line_no = 0;
    while (std::getline(file, line)) {
        ++line_no;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }
        // 切出操作码与剩余字段
        std::stringstream ss(t);
        std::string op;
        std::getline(ss, op, ',');
        op = trim(op);

        try {
            if (op == "ADD" || op == "A") {
                std::vector<std::string> fields;
                std::string field;
                while (std::getline(ss, field, ',')) {
                    fields.push_back(field);
                }
                add(parse_row(fields));
            } else if (op == "DEL" || op == "D") {
                std::string id_str;
                std::getline(ss, id_str, ',');
                remove(parse_int(id_str, "Flight ID"));
            } else if (op == "MOD" || op == "M") {
                std::vector<std::string> fields;
                std::string field;
                while (std::getline(ss, field, ',')) {
                    fields.push_back(field);
                }
                update(parse_row(fields));
            } else {
                throw std::invalid_argument("未知操作码: '" + op + "'");
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("批量操作第 " + std::to_string(line_no) + " 行出错: " + e.what());
        }
    }
}

} // namespace flight
