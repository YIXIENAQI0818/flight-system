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
    FlightDatabase _db;
    AirportService _airports;
    RouteService _routes;

    // —— 各实验功能入口 ——
    void _show_stats() const;
    void _query_direct() const;
    void _add_flight();
    void _remove_flight();
    void _modify_flight();
    void _batch_apply();
    void _suspend_resume();
    void _max_flights() const;
    void _search_airport() const;
    void _recommend_province() const;
    void _busiest_airport() const;
    void _connectivity() const;
    void _optimal_route() const;

    static void _print_flight(const Flight& f);
    static void _print_routes(const std::vector<std::vector<int>>& routes);
};

} // namespace flight

#endif // FLIGHT_CLI_H
