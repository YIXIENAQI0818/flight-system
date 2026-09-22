#include "api.h"

#include <stdexcept>
#include <string>

namespace flight {

namespace {

constexpr const char* kTimeFmt = "%m/%d/%Y %H:%M";

void send_json(httplib::Response& res, const nlohmann::json& j, int status = 200) {
    res.status = status;
    res.set_content(j.dump(), "application/json");
}

void send_error(httplib::Response& res, const std::string& msg, int status = 400) {
    send_json(res, {{"error", msg}}, status);
}

int int_param(const httplib::Request& req, const std::string& key, int fallback) {
    const std::string v = req.get_param_value(key);
    if (v.empty()) {
        return fallback;
    }
    return std::stoi(v);
}

std::optional<TimeWindow> window_from_params(const httplib::Request& req,
                                             const std::string& start_key,
                                             const std::string& end_key) {
    const std::string s = req.get_param_value(start_key);
    const std::string e = req.get_param_value(end_key);
    if (s.empty() && e.empty()) {
        return std::nullopt;
    }
    TimeWindow w;
    if (!s.empty()) {
        w.start = DateTime::parse(s, kTimeFmt);
    }
    if (!e.empty()) {
        w.end = DateTime::parse(e, kTimeFmt);
    }
    return w;
}

} // namespace

nlohmann::json to_json(const Flight& f) {
    return {
        {"id", f.id},
        {"flight_no", f.flight_no},
        {"is_international", f.is_international},
        {"from_airport", f.from_airport},
        {"to_airport", f.to_airport},
        {"dep_time", f.dep_time.to_string()},
        {"arr_time", f.arr_time.to_string()},
        {"airplane_id", f.airplane_id},
        {"airplane_model", f.airplane_model},
        {"fare", f.fare},
    };
}

nlohmann::json to_json(const Airport& a) {
    return {
        {"id", a.id},
        {"country", a.country},
        {"province", a.province},
        {"name", a.name},
        {"full_name", a.full_name()},
    };
}

Flight flight_from_json(const nlohmann::json& j) {
    Flight f;
    f.id = j.at("id").get<int>();
    f.flight_no = j.at("flight_no").get<int>();
    f.is_international = j.at("is_international").get<bool>();
    f.from_airport = j.at("from_airport").get<int>();
    f.to_airport = j.at("to_airport").get<int>();
    f.dep_time = DateTime::parse(j.at("dep_time").get<std::string>(), kTimeFmt);
    f.arr_time = DateTime::parse(j.at("arr_time").get<std::string>(), kTimeFmt);
    f.airplane_id = j.at("airplane_id").get<int>();
    f.airplane_model = j.at("airplane_model").get<int>();
    f.fare = j.at("fare").get<int>();
    f.date = DateTime(f.dep_time.year, f.dep_time.month, f.dep_time.day);
    return f;
}

void register_api(httplib::Server& server, FlightDatabase& db,
                  AirportService& airports, RouteService& routes) {
    // 实验一：统计
    server.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
        const FlightStats s = db.stats();
        auto opt = [](const std::optional<Flight>& o) -> nlohmann::json {
            return o ? to_json(*o) : nlohmann::json(nullptr);
        };
        send_json(res, {
            {"earliest_departure", opt(s.earliest_departure)},
            {"latest_departure", opt(s.latest_departure)},
            {"shortest_duration", opt(s.shortest_duration)},
            {"longest_duration", opt(s.longest_duration)},
            {"cheapest", opt(s.cheapest)},
            {"most_expensive", opt(s.most_expensive)},
        });
    });

    // 实验一：直达查询（from/to），否则返回（可 limit 截断的）全部活跃航班。
    server.Get("/api/flights", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const std::string from_s = req.get_param_value("from");
            const std::string to_s = req.get_param_value("to");
            std::vector<Flight> flights;
            if (!from_s.empty() && !to_s.empty()) {
                flights = db.direct_flights(std::stoi(from_s), std::stoi(to_s));
            } else {
                flights = db.active_flights();
                const std::string limit_s = req.get_param_value("limit");
                if (!limit_s.empty()) {
                    const size_t lim = std::stoul(limit_s);
                    if (flights.size() > lim) {
                        flights.resize(lim);
                    }
                }
            }
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& f : flights) {
                arr.push_back(to_json(f));
            }
            send_json(res, {{"count", flights.size()}, {"flights", arr}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    server.Get(R"(/api/flights/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        const int id = std::stoi(req.matches[1].str());
        const Flight* f = db.find_by_id(id);
        if (f == nullptr) {
            send_error(res, "航班 " + std::to_string(id) + " 不存在", 404);
            return;
        }
        send_json(res, to_json(*f));
    });

    // 实验二：增删改
    server.Post("/api/flights", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto j = nlohmann::json::parse(req.body);
            const Flight f = flight_from_json(j);
            db.add(f);
            send_json(res, to_json(f), 201);
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    server.Put(R"(/api/flights/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const int id = std::stoi(req.matches[1].str());
            Flight f = flight_from_json(nlohmann::json::parse(req.body));
            f.id = id;
            db.update(f);
            send_json(res, to_json(f));
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    server.Delete(R"(/api/flights/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            db.remove(std::stoi(req.matches[1].str()));
            send_json(res, {{"deleted", true}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    // 实验三：暂停 / 恢复
    server.Post("/api/airports/suspend", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const int id = nlohmann::json::parse(req.body).at("id").get<int>();
            db.suspend_airport(id);
            send_json(res, {{"airport_id", id}, {"suspended", true}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });
    server.Post("/api/airports/resume", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const int id = nlohmann::json::parse(req.body).at("id").get<int>();
            db.resume_airport(id);
            send_json(res, {{"airport_id", id}, {"suspended", false}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    // 实验三：最多乘机
    server.Get("/api/trips/max-flights", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const int start = int_param(req, "start", -1);
            const auto r = routes.max_flights(start);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& route : r.routes) {
                arr.push_back(route);
            }
            send_json(res, {{"max_count", r.max_count}, {"routes", arr}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    // 实验四：机场名搜索 / 同省推荐
    server.Get("/api/airports/search", [&](const httplib::Request& req, httplib::Response& res) {
        const std::string q = req.get_param_value("q");
        if (q.empty()) {
            send_error(res, "缺少参数 q");
            return;
        }
        const auto results = airports.search_by_name(q, 5);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : results) {
            arr.push_back({{"airport", to_json(r.airport)}, {"score", r.score}});
        }
        send_json(res, {{"results", arr}});
    });

    server.Get("/api/airports/recommend", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const int from = int_param(req, "from", -1);
            const int to = int_param(req, "to", -1);
            const auto rec = airports.recommend_same_province(db, from, to);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& f : rec) {
                arr.push_back(to_json(f));
            }
            send_json(res, {{"count", rec.size()}, {"flights", arr}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    // 实验五：最繁忙机场
    server.Get("/api/airports/busiest", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto dep_win = window_from_params(req, "dep_start", "dep_end");
            const auto arr_win = window_from_params(req, "arr_start", "arr_end");
            const auto traffic = airports.busiest_airports(db, dep_win, arr_win);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& t : traffic) {
                const Airport* a = airports.find_by_id(t.airport_id);
                arr.push_back({
                    {"airport_id", t.airport_id},
                    {"name", a ? a->name : ""},
                    {"full_name", a ? a->full_name() : ""},
                    {"departures", t.departures},
                    {"arrivals", t.arrivals},
                    {"total", t.total()},
                });
            }
            send_json(res, {{"busiest", arr}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    // 实验六：连通性
    server.Get("/api/routes/connect", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const int from = int_param(req, "from", -1);
            const int to = int_param(req, "to", -1);
            const int max_transfers = int_param(req, "max_transfers", 1);
            const auto dep_win = window_from_params(req, "dep_start", "dep_end");
            const auto arr_win = window_from_params(req, "arr_start", "arr_end");
            const auto result = routes.connectivity(from, to, max_transfers, dep_win, arr_win);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& route : result) {
                arr.push_back(route);
            }
            send_json(res, {{"count", result.size()}, {"routes", arr}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });

    // 实验六：最优方案
    server.Get("/api/routes/optimal", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const int from = int_param(req, "from", -1);
            const int to = int_param(req, "to", -1);
            std::string criteria = req.get_param_value("criteria");
            if (criteria.empty()) {
                criteria = "duration";
            }
            const Criterion c = parse_criterion(criteria);
            const auto result = routes.optimal_routes(from, to, c);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& route : result) {
                arr.push_back(route);
            }
            send_json(res, {{"count", result.size()}, {"routes", arr}});
        } catch (const std::exception& e) {
            send_error(res, e.what());
        }
    });
}

} // namespace flight
