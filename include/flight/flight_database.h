#ifndef FLIGHT_FLIGHT_DATABASE_H
#define FLIGHT_FLIGHT_DATABASE_H

#include "flight/flight.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flight {

// 实验一：数据统计结果。字段为空表示数据库为空（无对应统计）。
struct FlightStats {
    std::optional<Flight> earliest_departure;  // 起飞最早
    std::optional<Flight> latest_departure;    // 起飞最晚
    std::optional<Flight> shortest_duration;   // 飞行时间最短
    std::optional<Flight> longest_duration;    // 飞行时间最长
    std::optional<Flight> cheapest;            // 票价最低
    std::optional<Flight> most_expensive;      // 票价最高
};

// 实验一：直达航班的"最优标记"结果。三类各自可能有并列，故用 vector 存全部。
struct BestFlights {
    std::vector<Flight> cheapest;          // 费用最少
    std::vector<Flight> shortest_duration; // 用时最少
    std::vector<Flight> plus_one_day;      // 降落时间是起飞时间的次日
};

// 航班数据库：持有全部航班记录，负责加载、统计、直达查询、实时增删改、
// 文件批量操作，以及机场级暂停/恢复状态（实验一/二/三）。
//
// 约定：所有对外查询默认过滤"暂停航班"（其起降机场之一被暂停），
// 通过 active_flights() / is_flight_active() 体现；原始记录可通过 flights() 访问。
class FlightDatabase {
public:
    // 从 CSV 加载航班（覆盖式，重新初始化）。
    void load(const std::string& path);

    size_t size() const { return flights_.size(); }
    bool empty() const { return flights_.empty(); }

    // 原始全部航班（不过滤暂停）。
    const std::vector<Flight>& flights() const { return flights_; }
    // 活跃航班（过滤掉起降机场被暂停的航班）。
    std::vector<Flight> active_flights() const;

    const Flight* find_by_id(int id) const;

    // —— 实验二：实时增删改（非法操作抛出 std::invalid_argument）——
    void add(const Flight& f);          // id 已存在或字段非法 -> 抛异常
    void remove(int id);                // id 不存在 -> 抛异常
    void update(const Flight& f);       // 按 id 整体替换；id 不存在 -> 抛异常

    // —— 实验三：文件批量增删改。指令格式见 README，非法行抛异常并中止。——
    void batch_apply(const std::string& ops_path);

    // —— 实验三：机场暂停 / 恢复 ——
    void suspend_airport(int airport_id);
    void resume_airport(int airport_id);
    bool is_airport_suspended(int airport_id) const;
    bool is_flight_active(const Flight& f) const;
    const std::unordered_set<int>& suspended_airports() const { return suspended_airports_; }

    // —— 实验一：统计与直达查询 ——
    FlightStats stats() const;
    std::vector<Flight> direct_flights(int from, int to) const;
    BestFlights best_flights(int from, int to) const;

    // 从 CSV 行（11 字段）解析一条航班；解析失败抛异常。
    static Flight parse_row(const std::vector<std::string>& fields);

private:
    std::vector<Flight> flights_;
    std::unordered_map<int, size_t> id_to_index_;   // 航班 id -> 下标（加速查找）
    std::unordered_set<int> suspended_airports_;    // 被暂停的机场 id

    void rebuild_index();
};

} // namespace flight

#endif // FLIGHT_FLIGHT_DATABASE_H
