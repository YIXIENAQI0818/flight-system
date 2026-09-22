#ifndef FLIGHT_WEB_API_H
#define FLIGHT_WEB_API_H

#include "flight/airport_service.h"
#include "flight/flight_database.h"
#include "flight/route_service.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace flight {

// 将 Flight / Airport 序列化为 JSON（供 API 响应使用）。
nlohmann::json to_json(const Flight& f);
nlohmann::json to_json(const Airport& a);

// 从 JSON 解析一条航班（用于 POST / PUT）。字段非法抛 std::exception。
Flight flight_from_json(const nlohmann::json& j);

// 注册全部 REST API 路由到 server。
void register_api(httplib::Server& server, FlightDatabase& db,
                  AirportService& airports, RouteService& routes);

} // namespace flight

#endif // FLIGHT_WEB_API_H
