# ✈️ Flight System — 航班管理与航线搜索系统

一个基于 **C++17 + STL + 图算法** 的航班管理与航线搜索系统，完整覆盖数据结构课程"实验一~实验六"的全部功能，并提供**终端 CLI** 与 **网页版（Web 后端 + 浏览器前端）** 两个前端。

> 数据规模：2346 条航班、79 个机场（覆盖中国及部分国际航线）。

## 功能特性（对应六大实验）

| 实验 | 功能 | 说明 |
|------|------|------|
| **实验一** | 数据读取与统计 | 读取 `flights.csv`；统计起飞最早/最晚、飞行最短/最长、票价最低/最高；直达查询；标记 `cheapest / shortest duration / +1 day`（正确处理跨月、跨年的"次日到达"） |
| **实验二** | 航线实时增删改 | 单条航班的新增 / 删除 / 修改，非法操作给出明确错误提示 |
| **实验三** | 批量操作与状态管理 | 文件批量增删改；机场级暂停 / 恢复（极端天气等场景）；最多乘机问题（换乘需预留 ≥ 30 分钟，考虑暂停状态） |
| **实验四** | 机场搜索与推荐 | 机场名近似搜索（Top-5，按相似度降序）；同省机场推荐（无直达时） |
| **实验五** | 最繁忙机场统计 | 统计最繁忙机场及其起飞 / 降落航班数，支持起飞 / 降落时段过滤 |
| **实验六** | 连通性与最优方案 | 直飞 / 1 次中转的连通性（可限时段）；不限中转次数的最短飞行时间（含转机停留）/ 最低航费最优方案（输出全部并列最优） |

## 架构

采用「核心库 + 多前端」的分层设计，CLI 与 Web 复用同一套核心业务逻辑：

```
┌───────────────┬──────────────────────┐
│   flight-cli  │       flight-web     │   前端
│  (终端菜单)    │  (HTTP 服务 + 静态页) │
└───────┬───────┴──────────┬───────────┘
        │         依赖      │
        ▼                  ▼
┌─────────────────────────────────────┐
│            flight_core 核心库         │
│  FlightDatabase  数据加载/CRUD/暂停恢复│
│  RouteGraph + RouteService 图算法      │
│  AirportService 搜索/推荐/繁忙统计     │
│  DateTime / CsvReader 基础设施        │
└─────────────────────────────────────┘
```

### 核心算法

- **换乘图（Flight Graph）**：以航班为节点，边表示"可换乘"（到达机场 == 下一班起飞机场，且起飞时间 ≥ 到达时间 + 30 分钟）。时间严格递增 → 有向无环图（DAG）。
- **连通性**：DFS 枚举直飞 / 1 次中转的可行乘机方案。
- **最优方案**：DAG 上动态规划求最短路径，再沿"最短路边"回溯枚举全部最优方案（最短时间含转机停留 / 最低航费）。
- **最多乘机**：DAG 最长路径 DP + 全路径枚举。
- **机场名近似搜索**：字符集包含度（权重 0.3）+ 最长公共子串（权重 0.7）+ 精确匹配加成。

## 目录结构

```
flight-system/
├── CMakeLists.txt          # CMake 构建脚本
├── include/flight/         # 头文件（核心库接口）
├── src/                    # 核心库实现 + CLI 入口
├── web/                    # Web 后端（server/api）+ 静态前端
│   ├── server.cpp / api.cpp / api.h
│   └── static/             # index.html / style.css / app.js
├── data/                   # flights.csv（2346 条）、airports.csv（79 个）
└── tests/                  # GoogleTest 单元测试
```

## 构建与运行

依赖：C++17、CMake ≥ 3.16。GoogleTest / cpp-httplib / nlohmann-json 通过 CMake `FetchContent` 自动拉取（需联网）。

```bash
cmake -S . -B build          # 配置
cmake --build build -j       # 编译
ctest --test-dir build       # 运行单元测试
```

### 终端 CLI

```bash
./build/flight-cli           # 进入交互式菜单
./build/flight-cli data/flights.csv data/airports.csv   # 可指定数据路径
```

### 网页版

```bash
./build/flight-web 8080      # 启动 Web 服务（默认端口 8080）
```

浏览器访问 `http://localhost:8080`，即可在网页上操作六大实验的全部功能。

## REST API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/stats` | 实验一统计数据 |
| GET | `/api/flights?from=&to=` | 直达航班查询（无参数时返回全部活跃航班，支持 `limit`） |
| GET/POST/PUT/DELETE | `/api/flights[/:id]` | 查询 / 新增 / 修改 / 删除航班 |
| POST | `/api/airports/suspend` `/api/airports/resume` | 机场暂停 / 恢复（body `{"id": N}`） |
| GET | `/api/airports/search?q=` | 机场名近似搜索 Top-5 |
| GET | `/api/airports/recommend?from=&to=` | 同省机场推荐 |
| GET | `/api/airports/busiest?dep_start=&dep_end=&arr_start=&arr_end=` | 最繁忙机场（时段可选） |
| GET | `/api/routes/connect?from=&to=&max_transfers=` | 连通性查询 |
| GET | `/api/routes/optimal?from=&to=&criteria=duration\|fare` | 最优乘机方案 |
| GET | `/api/trips/max-flights?start=` | 最多乘机 |

时间参数格式：`MM/DD/YYYY HH:MM`（URL 中空格用 `%20` 或 `+` 编码）。

## 批量操作文件格式（实验三）

每行一条操作，`#` 开头为注释，空行忽略：

```text
# 新增航班（11 字段，与 flights.csv 行一致）
ADD,10001,5/5/2017,Dome,999,48,50,5/5/2017 12:20,5/5/2017 15:10,30,1,666

# 删除航班
DEL,10001

# 修改航班（按 ID 整体替换）
MOD,10001,5/6/2017,Dome,999,48,50,5/6/2017 12:20,5/6/2017 15:10,30,1,700
```

## 数据说明

- `data/flights.csv`：航班数据，字段为 `Flight ID, Departure date, Intl/Dome, Flight NO., Departure airport, Arrival airport, Departure Time, Arrival Time, Airplane ID, Airplane Model, Air fares`。日期格式 `MM/DD/YYYY`，时间格式 `MM/DD/YYYY HH:MM`。
- `data/airports.csv`：机场映射，字段为 `ID, Country, Province, Name`。

## 换乘规则

任意一次换乘需满足：下一班航班起飞时间 ≥ 上一班到达时间 + **30 分钟**（与实验三约定一致，实验六同样适用）。可通过 `RouteGraph::build` / `RouteService` 的 `min_connection_minutes` 参数调整。

## 技术栈

- **语言**：C++17
- **数据结构**：`std::vector` / `std::unordered_map` / `std::unordered_set` / `std::priority_queue`（STL）
- **算法**：DAG 拓扑序、动态规划、Dijkstra 思想、DFS 枚举、最长公共子串、字符串相似度
- **Web**：cpp-httplib（HTTP）、nlohmann/json（序列化）
- **测试**：GoogleTest + CTest
- **构建**：CMake

## License

MIT
