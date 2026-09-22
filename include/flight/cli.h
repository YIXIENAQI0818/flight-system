#ifndef FLIGHT_CLI_H
#define FLIGHT_CLI_H

#include "flight/airport_service.h"
#include "flight/flight_database.h"
#include "flight/route_service.h"

#include <string>

namespace flight {

// 交互式命令行前端：加载数据后进入菜单循环，覆盖实验一~实验六全部功能。
class Cli {
public:
    // flights_path / airports_path 为 CSV 路径（加载失败抛异常）。
    Cli(std::string flights_path, std::string airports_path);

    // 运行菜单循环，返回进程退出码。
    int run();

private:
    FlightDatabase db_;
    AirportService airports_;
    RouteService routes_;

    // —— 各实验功能入口 ——
    void show_stats() const;
    void query_direct() const;
    void add_flight();
    void remove_flight();
    void modify_flight();
    void batch_apply();
    void suspend_resume();
    void max_flights() const;
    void search_airport() const;
    void recommend_province() const;
    void busiest_airport() const;
    void connectivity() const;
    void optimal_route() const;

    static void print_flight(const Flight& f);
    static void print_routes(const std::vector<std::vector<int>>& routes);
};

} // namespace flight

#endif // FLIGHT_CLI_H
