cmake_minimum_required(VERSION 3.16)
project(flight-system VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FLIGHT_BUILD_WEB "构建 Web 后端" ON)
option(FLIGHT_BUILD_TESTS "构建单元测试" ON)

# 核心库源文件（不含前端入口）
set(FLIGHT_CORE_SOURCES
    src/date_time.cpp
    src/csv_reader.cpp
    src/flight_database.cpp
    src/route_graph.cpp
    src/route_service.cpp
    src/airport_service.cpp
)

add_library(flight_core STATIC ${FLIGHT_CORE_SOURCES})
target_include_directories(flight_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(flight_core PUBLIC cxx_std_17)
target_compile_definitions(flight_core PUBLIC FLIGHT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
if(MSVC)
    target_compile_options(flight_core PRIVATE /W4)
else()
    target_compile_options(flight_core PRIVATE -Wall -Wextra)
endif()

# —— 终端 CLI 前端 ——
add_executable(flight-cli src/main_cli.cpp)
target_link_libraries(flight-cli PRIVATE flight_core)

# —— Web 后端 + 前端 ——
if(FLIGHT_BUILD_WEB)
    include(FetchContent)
    FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(httplib json)

    add_executable(flight-web web/server.cpp web/api.cpp)
    target_link_libraries(flight-web PRIVATE flight_core httplib::httplib nlohmann_json::nlohmann_json)
    target_compile_definitions(flight-web PRIVATE
        FLIGHT_STATIC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/web/static")
endif()

# —— GoogleTest 单元测试 ——
if(FLIGHT_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set(FLIGHT_TEST_SOURCES
        tests/test_date_time.cpp
        tests/test_csv_reader.cpp
        tests/test_flight_database.cpp
        tests/test_route_service.cpp
        tests/test_airport_service.cpp
    )
    add_executable(flight-tests ${FLIGHT_TEST_SOURCES})
    target_link_libraries(flight-tests PRIVATE flight_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(flight-tests)
endif()
