#include "flight/cli.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace flight {

namespace {

std::string read_line(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

bool parse_int(const std::string& s, int& out) {
    try {
        size_t pos = 0;
        out = std::stoi(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

int read_int(const std::string& prompt) {
    while (true) {
        int v = 0;
        if (parse_int(read_line(prompt), v)) {
            return v;
        }
        std::cout << "输入不是合法整数，请重试。" << std::endl;
    }
}

DateTime read_datetime(const std::string& prompt) {
    while (true) {
        const std::string s = read_line(prompt);
        try {
            return DateTime::parse(s, "%m/%d/%Y %H:%M");
        } catch (...) {
            try {
                return DateTime::parse(s, "%m/%d/%Y");
            } catch (...) {
                std::cout << "格式错误，请使用 MM/DD/YYYY HH:MM。" << std::endl;
            }
        }
    }
}

std::optional<TimeWindow> read_window(const std::string& what) {
    while (true) {
        const std::string ans = read_line("是否限定" + what + "时段? (y/n): ");
        if (ans == "y" || ans == "Y") {
            const DateTime s = read_datetime("请输入" + what + "时段起点 (MM/DD/YYYY HH:MM): ");
            const DateTime e = read_datetime("请输入" + what + "时段终点 (MM/DD/YYYY HH:MM): ");
            return TimeWindow{s, e};
        }
        if (ans == "n" || ans == "N") {
            return std::nullopt;
        }
        std::cout << "请输入 y 或 n。" << std::endl;
    }
}

bool confirm(const std::string& prompt) {
    const std::string ans = read_line(prompt);
    return ans == "y" || ans == "Y" || ans == "yes" || ans == "YES";
}

void print_separator() {
    std::cout << "--------------------------------------------------------------" << std::endl;
}

} // namespace

Cli::Cli(std::string flights_path, std::string airports_path) : _routes(_db) {
    _db.load(flights_path);
    _airports.load(airports_path);
    std::cout << "已加载航班 " << _db.size() << " 条，机场 " << _airports.airports().size()
              << " 个。" << std::endl;
}

void Cli::_print_flight(const Flight& f) {
    std::cout << "  ID=" << f.id
              << " 航班号=" << f.flight_no
              << " 类型=" << (f.is_international ? "Intl" : "Dome")
              << " 起降=" << f.from_airport << "->" << f.to_airport
              << " 起飞=" << f.dep_time.to_string()
              << " 到达=" << f.arr_time.to_string()
              << " 机型=" << f.airplane_model
              << " 票价=" << f.fare << std::endl;
}

void Cli::_print_routes(const std::vector<std::vector<int>>& routes) {
    if (routes.empty()) {
        std::cout << "  无可行方案。" << std::endl;
        return;
    }
    for (size_t i = 0; i < routes.size(); ++i) {
        std::cout << "  方案 " << (i + 1) << ": ";
        for (size_t j = 0; j < routes[i].size(); ++j) {
            std::cout << routes[i][j] << (j + 1 < routes[i].size() ? " -> " : "");
        }
        std::cout << std::endl;
    }
}

void Cli::_show_stats() const {
    const FlightStats s = _db.stats();
    print_separator();
    std::cout << "【实验一】数据统计" << std::endl;
    if (s.earliest_departure) {
        std::cout << "起飞最早: ";
        _print_flight(*s.earliest_departure);
        std::cout << "起飞最晚: ";
        _print_flight(*s.latest_departure);
        std::cout << "飞行最短: ";
        _print_flight(*s.shortest_duration);
        std::cout << "飞行最长: ";
        _print_flight(*s.longest_duration);
        std::cout << "票价最低: ";
        _print_flight(*s.cheapest);
        std::cout << "票价最高: ";
        _print_flight(*s.most_expensive);
    } else {
        std::cout << "  无活跃航班数据。" << std::endl;
    }
}

void Cli::_query_direct() const {
    print_separator();
    std::cout << "【实验一】直达航班查询与标记" << std::endl;
    int from = read_int("请输入出发机场 ID: ");
    int to = read_int("请输入到达机场 ID: ");
    const auto direct = _db.direct_flights(from, to);
    if (direct.empty()) {
        std::cout << "两机场之间没有直达航班。" << std::endl;
        return;
    }
    std::cout << "直达航班共 " << direct.size() << " 条：" << std::endl;
    for (const auto& f : direct) {
        _print_flight(f);
    }
    const BestFlights best = _db.best_flights(from, to);
    std::cout << "标记 cheapest: ";
    for (const auto& f : best.cheapest) {
        std::cout << f.id << " ";
    }
    std::cout << std::endl << "标记 shortest duration: ";
    for (const auto& f : best.shortest_duration) {
        std::cout << f.id << " ";
    }
    std::cout << std::endl << "标记 +1 day: ";
    for (const auto& f : best.plus_one_day) {
        std::cout << f.id << " ";
    }
    std::cout << std::endl;
}

void Cli::_add_flight() {
    print_separator();
    std::cout << "【实验二】增加航班（输入 11 字段，逗号分隔，格式同 CSV）" << std::endl;
    std::cout << "Flight ID,Departure date,Intl/Dome,Flight NO.,Departure airport,"
                 "Arrival airport,Departure Time,Arrival Time,Airplane ID,Airplane Model,Air fares"
              << std::endl;
    const std::string line = read_line("> ");
    std::stringstream ss(line);
    std::vector<std::string> fields;
    std::string f;
    while (std::getline(ss, f, ',')) {
        fields.push_back(f);
    }
    try {
        _db.add(FlightDatabase::parse_row(fields));
        std::cout << "添加成功。" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "添加失败: " << e.what() << std::endl;
    }
}

void Cli::_remove_flight() {
    print_separator();
    std::cout << "【实验二】删除航班" << std::endl;
    const int id = read_int("请输入要删除的航班 ID: ");
    try {
        _db.remove(id);
        std::cout << "删除成功。" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "删除失败: " << e.what() << std::endl;
    }
}

void Cli::_modify_flight() {
    print_separator();
    std::cout << "【实验二】修改航班（按 ID 整体替换，输入 11 字段，逗号分隔）" << std::endl;
    std::cout << "Flight ID,Departure date,Intl/Dome,Flight NO.,Departure airport,"
                 "Arrival airport,Departure Time,Arrival Time,Airplane ID,Airplane Model,Air fares"
              << std::endl;
    const std::string line = read_line("> ");
    std::stringstream ss(line);
    std::vector<std::string> fields;
    std::string f;
    while (std::getline(ss, f, ',')) {
        fields.push_back(f);
    }
    try {
        _db.update(FlightDatabase::parse_row(fields));
        std::cout << "修改成功。" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "修改失败: " << e.what() << std::endl;
    }
}

void Cli::_batch_apply() {
    print_separator();
    std::cout << "【实验三】文件批量增删改（指令格式见 README）" << std::endl;
    const std::string path = read_line("请输入批量操作文件路径: ");
    try {
        _db.batch_apply(path);
        std::cout << "批量操作完成，当前航班数 " << _db.size() << "。" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "批量操作失败: " << e.what() << std::endl;
    }
}

void Cli::_suspend_resume() {
    print_separator();
    std::cout << "【实验三】机场暂停 / 恢复" << std::endl;
    std::cout << "  1. 暂停某机场所有起降航班" << std::endl;
    std::cout << "  2. 恢复某机场所有起降航班" << std::endl;
    const int choice = read_int("请选择: ");
    const int id = read_int("请输入机场 ID: ");
    if (choice == 1) {
        _db.suspend_airport(id);
        std::cout << "机场 " << id << " 已暂停。" << std::endl;
    } else if (choice == 2) {
        _db.resume_airport(id);
        std::cout << "机场 " << id << " 已恢复。" << std::endl;
    } else {
        std::cout << "无效选择。" << std::endl;
    }
}

void Cli::_max_flights() const {
    print_separator();
    std::cout << "【实验三】最多乘机问题（换乘需 ≥ 30 分钟）" << std::endl;
    const int id = read_int("请输入起始航班 ID: ");
    const auto res = _routes.max_flights(id);
    if (res.max_count == 0) {
        std::cout << "起始航班不存在或已被暂停。" << std::endl;
        return;
    }
    std::cout << "最多可乘坐 " << res.max_count << " 次航班，共 " << res.routes.size()
              << " 条最长路线：" << std::endl;
    _print_routes(res.routes);
}

void Cli::_search_airport() const {
    print_separator();
    std::cout << "【实验四】机场名称近似搜索" << std::endl;
    const std::string query = read_line("请输入关键词: ");
    const auto results = _airports.search_by_name(query, 5);
    std::cout << "最相似的 5 个机场：" << std::endl;
    for (const auto& r : results) {
        std::cout << "  " << r.airport.full_name() << " (相似度 " << r.score << ")" << std::endl;
    }
}

void Cli::_recommend_province() const {
    print_separator();
    std::cout << "【实验四】同省机场推荐" << std::endl;
    int from = read_int("请输入出发机场 ID: ");
    int to = read_int("请输入到达机场 ID: ");
    const auto direct = _db.direct_flights(from, to);
    if (!direct.empty()) {
        std::cout << "两机场之间有直达航班，无需推荐：" << std::endl;
        for (const auto& f : direct) {
            _print_flight(f);
        }
        return;
    }
    const auto rec = _airports.recommend_same_province(_db, from, to);
    if (rec.empty()) {
        std::cout << "同省机场之间也没有可推荐的直达航班。" << std::endl;
        return;
    }
    std::cout << "同省机场直达航班推荐（共 " << rec.size() << " 条）：" << std::endl;
    for (const auto& f : rec) {
        _print_flight(f);
    }
}

void Cli::_busiest_airport() const {
    print_separator();
    std::cout << "【实验五】最繁忙机场统计" << std::endl;
    const bool limited = confirm("是否限定起飞/降落时段? (y/n): ");
    std::optional<TimeWindow> dep_win, arr_win;
    if (limited) {
        dep_win = read_window("起飞");
        arr_win = read_window("降落");
    }
    const auto traffic = _airports.busiest_airports(_db, dep_win, arr_win);
    if (traffic.empty()) {
        std::cout << "无航班数据。" << std::endl;
        return;
    }
    std::cout << "最繁忙机场：" << std::endl;
    for (const auto& t : traffic) {
        const Airport* a = _airports.find_by_id(t.airport_id);
        const std::string name = a ? a->name : std::to_string(t.airport_id);
        std::cout << "  " << name << " (ID=" << t.airport_id << "): 起飞 " << t.departures
                  << " 班，降落 " << t.arrivals << " 班，合计 " << t.total() << " 班。" << std::endl;
    }
}

void Cli::_connectivity() const {
    print_separator();
    std::cout << "【实验六】连通性查询（直飞或 1 次中转）" << std::endl;
    const int from = read_int("请输入出发机场 ID: ");
    const int to = read_int("请输入到达机场 ID: ");
    const auto dep_win = read_window("起飞");
    const auto arr_win = read_window("降落");
    const auto routes = _routes.connectivity(from, to, 1, dep_win, arr_win);
    std::cout << "可行乘机方案共 " << routes.size() << " 条：" << std::endl;
    _print_routes(routes);
}

void Cli::_optimal_route() const {
    print_separator();
    std::cout << "【实验六】最优乘机方案" << std::endl;
    const int from = read_int("请输入出发机场 ID: ");
    const int to = read_int("请输入到达机场 ID: ");
    std::cout << "  1. 最短飞行时间（含转机停留）" << std::endl;
    std::cout << "  2. 最低航费" << std::endl;
    const int c = read_int("请选择: ");
    const Criterion criterion = (c == 2) ? Criterion::Fare : Criterion::Duration;
    const auto routes = _routes.optimal_routes(from, to, criterion);
    std::cout << "最优方案共 " << routes.size() << " 条：" << std::endl;
    _print_routes(routes);
}

int Cli::run() {
    while (true) {
        print_separator();
        std::cout << "航班管理与航线搜索系统 - 菜单" << std::endl;
        std::cout << "  [实验一] 1.统计  2.直达查询与标记" << std::endl;
        std::cout << "  [实验二] 3.增加  4.删除  5.修改" << std::endl;
        std::cout << "  [实验三] 6.批量增删改  7.暂停/恢复  8.最多乘机" << std::endl;
        std::cout << "  [实验四] 9.机场名搜索  10.同省推荐" << std::endl;
        std::cout << "  [实验五] 11.最繁忙机场" << std::endl;
        std::cout << "  [实验六] 12.连通性  13.最优方案" << std::endl;
        std::cout << "  0.退出" << std::endl;
        const int choice = read_int("请输入选择: ");
        switch (choice) {
            case 0: std::cout << "再见！" << std::endl; return 0;
            case 1: _show_stats(); break;
            case 2: _query_direct(); break;
            case 3: _add_flight(); break;
            case 4: _remove_flight(); break;
            case 5: _modify_flight(); break;
            case 6: _batch_apply(); break;
            case 7: _suspend_resume(); break;
            case 8: _max_flights(); break;
            case 9: _search_airport(); break;
            case 10: _recommend_province(); break;
            case 11: _busiest_airport(); break;
            case 12: _connectivity(); break;
            case 13: _optimal_route(); break;
            default: std::cout << "无效选择，请重试。" << std::endl; break;
        }
    }
}

} // namespace flight

#ifndef FLIGHT_DATA_DIR
#define FLIGHT_DATA_DIR "data"
#endif

int main(int argc, char* argv[]) {
    const std::string flights_path =
        argc > 1 ? argv[1] : std::string(FLIGHT_DATA_DIR) + "/flights.csv";
    const std::string airports_path =
        argc > 2 ? argv[2] : std::string(FLIGHT_DATA_DIR) + "/airports.csv";
    try {
        flight::Cli cli(flights_path, airports_path);
        return cli.run();
    } catch (const std::exception& e) {
        std::cerr << "启动失败: " << e.what() << std::endl;
        return 1;
    }
}
