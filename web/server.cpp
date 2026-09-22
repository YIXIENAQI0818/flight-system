#include "api.h"

#include <httplib.h>

#include <iostream>
#include <stdexcept>
#include <string>

#ifndef FLIGHT_DATA_DIR
#define FLIGHT_DATA_DIR "data"
#endif
#ifndef FLIGHT_STATIC_DIR
#define FLIGHT_STATIC_DIR "web/static"
#endif

int main(int argc, char* argv[]) {
    const int port = argc > 1 ? std::stoi(argv[1]) : 8080;
    const std::string data_dir = argc > 2 ? argv[2] : FLIGHT_DATA_DIR;
    const std::string static_dir = argc > 3 ? argv[3] : FLIGHT_STATIC_DIR;

    flight::FlightDatabase db;
    flight::AirportService airports;
    flight::RouteService routes(db);

    try {
        db.load(data_dir + "/flights.csv");
        airports.load(data_dir + "/airports.csv");
    } catch (const std::exception& e) {
        std::cerr << "数据加载失败: " << e.what() << std::endl;
        return 1;
    }

    httplib::Server server;
    server.set_mount_point("/", static_dir);
    flight::register_api(server, db, airports, routes);

    std::cout << "航班系统 Web 服务已启动: http://localhost:" << port << std::endl;
    std::cout << "  - 前端页面: http://localhost:" << port << "/" << std::endl;
    std::cout << "  - 统计接口: http://localhost:" << port << "/api/stats" << std::endl;

    if (!server.listen("0.0.0.0", port)) {
        std::cerr << "监听端口 " << port << " 失败" << std::endl;
        return 1;
    }
    return 0;
}
