# flight-system 整体技术报告

> 报告日期：2026-09-23 | 作者：程宣赫

---

## 目录

1. [项目概述](#1-项目概述)
2. [基础设施层：DateTime / CsvReader](#2-基础设施层datetime--csvreader)
3. [数据层：FlightDatabase（实验一二三）](#3-数据层flightdatabase实验一二三)
4. [图算法层：FlightGraph + RouteService（实验三六）](#4-图算法层flightgraph--routeservice实验三六)
5. [机场服务层：AirportService（实验四五）](#5-机场服务层airportservice实验四五)
6. [CLI 前端](#6-cli-前端)
7. [Web 前端](#7-web-前端)
8. [架构全景图](#8-架构全景图)
9. [关键设计决策汇总](#9-关键设计决策汇总)

---

## 1. 项目概述

`flight-system` 是一个基于 **C++17 + STL + 图算法** 的航班管理与航线搜索系统，完整覆盖数据结构课程「实验一~实验六」的全部功能。核心库 `flight_core` 承载全部业务逻辑（纯逻辑、无 I/O 耦合、可单元测试），其上架了两个前端：**终端 CLI** 与 **网页版（HTTP 后端 + 静态前端）**，二者复用同一套核心库。

> 数据规模：**2346 条航班、79 个机场**（覆盖中国及部分国际航线）。

### 1.1 分层架构总览

```
┌────────────────────┬────────────────────────┐
│    flight-cli      │       flight-web       │  前端
│   (终端交互菜单)     │  (HTTP 服务 + 静态页面)  │
└─────────┬──────────┴───────────┬────────────┘
          │        依赖           │
          ▼                      ▼
┌───────────────────────────────────────────────────┐
│                 flight_core 核心库                 │
│                                                   │
│   AirportService  搜索/推荐/繁忙统计（实验四/五）     │
│   RouteGraph + RouteService  图算法（实验三/六）     │
│   FlightDatabase  加载/统计/CRUD/批量/暂停（实验一~三）│
│   DateTime / CsvReader  基础设施                    │
└───────────────────────────────────────────────────┘
```

### 1.2 六大实验映射

| 实验 | 功能 | 承载模块 |
|------|------|---------|
| **实验一** | 数据读取与统计 / 直达查询与标记 | `FlightDatabase`（`stats` / `best_flights` / `direct_flights`） |
| **实验二** | 航班实时增删改 | `FlightDatabase`（`add` / `remove` / `update`） |
| **实验三** | 批量操作、机场暂停/恢复、最多乘机 | `FlightDatabase`（`batch_apply` / suspend）+ `RouteService`（`max_flights`） |
| **实验四** | 机场名近似搜索、同省机场推荐 | `AirportService`（`search_by_name` / `recommend_same_province`） |
| **实验五** | 最繁忙机场统计（含时段过滤） | `AirportService`（`busiest_airports`） |
| **实验六** | 连通性、最优乘机方案 | `RouteService`（`connectivity` / `optimal_routes`） |

### 1.3 技术栈

| 组件 | 技术选型 | 说明 |
|------|---------|------|
| 语言 | C++17 | `CMAKE_CXX_STANDARD 17`，禁用编译器扩展 |
| 数据结构 | STL（`vector` / `unordered_map` / `unordered_set` / `array`） | 不手写容器 |
| 算法 | DAG 拓扑序、动态规划、DFS 枚举、最长公共子串、字符串相似度 | 核心见第 4 章 |
| HTTP | cpp-httplib v0.15.3 | header-only，`FetchContent` 拉取 |
| JSON | nlohmann/json v3.11.3 | 序列化 / 反序列化 |
| 测试 | GoogleTest + CTest | 45 个用例 |
| 构建 | CMake ≥ 3.16 | 静态库 `flight_core` + 两个可执行文件 |

---

## 2. 基础设施层：DateTime / CsvReader

### 2.1 定位

这一层是系统的「地基」，被所有上层模块依赖。核心目标是建立一个**可比较、可测试、无时区陷阱**的日期时间值类型，以及一个简单可靠的 CSV 读取工具。

### 2.2 DateTime — 日期时间值类型

```cpp
class DateTime {
public:
    int year = 0, month = 0, day = 0, hour = 0, minute = 0;

    static DateTime parse(const std::string& s, const std::string& fmt);
    long long total_minutes() const;   // 距 1970-01-01 00:00 的总分钟数
    std::string to_string() const;     // "MM/DD/YYYY HH:MM"
    int day_number() const;            // 距 1970-01-01 的天数

    // 六种比较运算符全部基于 total_minutes()
};
```

#### 2.2.1 为什么用「总分钟数」做比较

`std::tm` 是一个「裸字段结构体」，既不提供比较运算符，也无法直接做差值；而直接用 `year/month/day/hour/minute` 逐字段比较又啰嗦易错。`DateTime` 的解法是引入一个**规范表示**——`total_minutes()`（距 1970-01-01 00:00 的总分钟数，可为负）：

```cpp
long long DateTime::total_minutes() const {
    return static_cast<long long>(day_number()) * 24 * 60 + hour * 60 + minute;
}
```

所有比较运算符（`==` / `<` / `<=` …）和差值运算（`minutes_between`）都退化到这个一维整数上。这样：

- **比较**：`a < b` 直接变成整数比较，天然正确。
- **差值**：`b - a` 的分钟数是一条整数减法，跨天、跨月、跨年都成立。
- **规避时区陷阱**：解析基于 `std::get_time`，但一旦得到字段就立即折算为绝对分钟数，后续运算不再依赖 `std::tm` 的时区语义。

#### 2.2.2 日期折算 — Howard Hinnant 算法

`day_number()` 把公历日期折算为「距 1970-01-01 的天数」，采用的是 Howard Hinnant 的 `days_from_civil` 算法（proleptic Gregorian，即把格里高利历向前无限延伸）：

```cpp
int days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);      // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;     // [0, 146096]
    return era * 146097 + static_cast<int>(doe) - 719468;
}
```

这个算法把一个「年月日」线性映射到一个整数「第几天」，是 C++20 `std::chrono::day` 同源的经典实现。它在本项目里解决一个具体问题：**`is_next_day(a, b)` 判断"次日到达"**。

```cpp
bool is_next_day(const DateTime& a, const DateTime& b) {
    return b.day_number() == a.day_number() + 1;
}
```

实验一要求标记「降落时间是起飞时间的次日」（`+1 day`）的航班。如果只比较 `b.day == a.day + 1`，在月末（`5/31` → `6/1`）或年末（`12/31` → `1/1`）会判断错误；而 `day_number()` 是线性天数，`+ 1` 天然跨过所有月/年边界。

#### 2.2.3 解析与格式化

解析统一走 `std::get_time`，格式串由调用方传入（`"%m/%d/%Y"` 或 `"%m/%d/%Y %H:%M"`）：

```cpp
DateTime DateTime::parse(const std::string& s, const std::string& fmt) {
    std::tm tm{};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, fmt.c_str());
    if (ss.fail()) {
        throw std::runtime_error("无法解析日期时间: '" + s + "' (格式 " + fmt + ")");
    }
    // tm_year 是"自 1900"，tm_mon 是 0-based，需换算
    ...
}
```

**设计要点**：

- **失败即抛异常**：`std::get_time` 解析失败会置 `failbit`，直接抛 `std::runtime_error`，让上层（`parse_row`、CLI 输入循环）决定如何处理。这比返回默认值更能暴露数据错误。
- **`tm_year + 1900` / `tm_mon + 1`**：`std::tm` 的这两个字段分别是「自 1900 起的年数」和「0-based 月」，必须换算，否则日期会整体偏移。
- **`to_string()` 补零**：`setw(2) + setfill('0')` 保证 `5/5/2017 9:05` 输出为 `05/05/2017 09:05`，与输入格式对齐，便于 JSON 序列化和回显。

### 2.3 CsvReader — CSV 读取工具

```cpp
class CsvReader {
public:
    using Row = std::vector<std::string>;
    using Table = std::vector<Row>;
    static Table read(const std::string& path, bool skip_header = true);
};
```

实现刻意保持极简：逐行 `getline`，按 `,` 切分字段，`skip_header` 时丢弃首行，空行忽略，打开失败抛异常。它**不做类型转换、不做转义处理**——那是 `parse_row` 的职责。这个职责边界很关键：CSV 读取只负责「文本 → 字段数组」，字段语义（哪些是 int、哪些是日期）交给下游，保证 `CsvReader` 可以被 `FlightDatabase::load` 和 `AirportService::load` 两个完全不同的数据源复用。

---

## 3. 数据层：FlightDatabase（实验一/二/三）

### 3.1 定位

`FlightDatabase` 是航班数据的唯一持有者，承载实验一（统计/直达）、实验二（增删改）、实验三（批量/暂停恢复）的全部状态与操作。它是「有状态」的核心：不仅存航班，还维护 **id 索引** 和 **暂停机场集合** 两份派生状态。

### 3.2 数据模型 — `Flight`

```cpp
struct Flight {
    int id = 0;                 // 航班 ID
    DateTime date;              // 起飞日期
    bool is_international = false; // Intl / Dome
    int flight_no = 0;          // 航班编号
    int from_airport = 0;       // 起飞机场 ID
    int to_airport = 0;         // 到达机场 ID
    DateTime dep_time;          // 起飞时间
    DateTime arr_time;          // 到达时间
    int airplane_id = 0;        // 飞机 ID
    int airplane_model = 0;     // 飞机机型
    int fare = 0;               // 基础票价

    int duration_minutes() const { return minutes_between(dep_time, arr_time); }
};
```

字段顺序与 `flights.csv` 的 11 列一一对应。`duration_minutes()` 是飞行时长的便捷访问，被统计（`shortest_duration` / `longest_duration`）和最优方案（`Criterion::Duration`）反复使用。

### 3.3 解析与校验 — `parse_row`

```cpp
Flight FlightDatabase::parse_row(const std::vector<std::string>& fields) {
    if (fields.size() < 11) { throw ...; }
    Flight f;
    f.id = parse_int(fields[0], "Flight ID");
    f.date = DateTime::parse(trim(fields[1]), kDateFmt);
    f.is_international = (trim(fields[2]) == "Intl");
    ...
    // 语义校验
    if (f.from_airport == f.to_airport) { throw ...; }   // 起降机场相同
    if (f.dep_time >= f.arr_time) { throw ...; }         // 起飞不早于到达
    if (f.fare < 0) { throw ...; }                       // 票价为负
    return f;
}
```

**两个层次的校验**值得注意：

- **格式校验**（`parse_int` / `DateTime::parse`）：字段不是合法整数或日期时抛 `std::invalid_argument`，错误信息带字段名和原始值。
- **语义校验**（起降机场相同、起飞不早于到达、票价为负）：这是「数据不变量」，在任何入口（`load` 解析、`add`、`update`）都必须守住。因此这三条校验在 `parse_row`、`add`、`update` 里**重复出现**——不是冗余，而是因为 `add` 接收的是已构造好的 `Flight` 对象，可能绕过 `parse_row`（Web 端 `flight_from_json` 就是另一个构造路径）。

`parse_row` 是 `static`，被 `load`、`batch_apply`、CLI 的 `add_flight`/`modify_flight` 共同复用，是「一处定义、多处消费」的典型。

### 3.4 索引与增删改

#### 3.4.1 id 哈希索引

```cpp
std::vector<Flight> flights_;
std::unordered_map<int, size_t> id_to_index_;   // 航班 id -> 下标
```

航班存在 `vector` 里（顺序存储、可整体遍历），同时维护 `id -> 下标` 的哈希表，把 `find_by_id` 从 O(n) 降为 O(1)。增删改后通过 `rebuild_index()` 重建索引。

#### 3.4.2 `remove` 的 swap-and-pop

```cpp
void FlightDatabase::remove(int id) {
    const auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) { throw ...; }
    const size_t idx = it->second;
    flights_[idx] = flights_.back();   // 用末尾元素覆盖被删位置
    flights_.pop_back();               // 弹出末尾
    rebuild_index();
}
```

删除不用 `erase`（那会导致后续所有元素前移，O(n)），而是把**末尾元素搬到被删位置再 `pop_back`**，O(1) 完成。代价是打乱了存储顺序，所以紧接着 `rebuild_index()` 重建 id 索引。这个取舍是合理的：本系统对航班**没有「保持插入顺序」的需求**（统计、搜索都遍历全部或走索引），换来 O(1) 删除。

### 3.5 暂停语义（实验三）

```cpp
std::unordered_set<int> suspended_airports_;    // 被暂停的机场 id

bool FlightDatabase::is_flight_active(const Flight& f) const {
    return !is_airport_suspended(f.from_airport) && !is_airport_suspended(f.to_airport);
}
```

机场暂停用于模拟「极端天气等导致机场关闭」的场景。**暂停语义的核心约定**是：某机场被暂停后，其**起降**航班一律视为不活跃——航班只要起飞机场或到达机场之一被暂停，就从所有对外查询中消失。这个语义通过一个统一入口贯彻：

```cpp
std::vector<Flight> FlightDatabase::active_flights() const {
    std::vector<Flight> result;
    for (const auto& f : flights_) {
        if (is_flight_active(f)) result.push_back(f);
    }
    return result;
}
```

**关键设计**：`flights()` 返回**原始全部**航班，`active_flights()` 返回**过滤后的活跃**航班。所有「对外查询」（`stats`、`direct_flights`、`RouteGraph::build` 的输入、`busiest_airports`）都走 `active_flights()` / `is_flight_active()`，而 CRUD 操作作用于原始 `flights_`。这样暂停状态是**非破坏性**的——恢复机场后，航班原封不动地重新活跃。

### 3.6 统计 — `stats()`（实验一）

```cpp
FlightStats FlightDatabase::stats() const {
    FlightStats s;
    for (const auto& f : flights_) {
        if (!is_flight_active(f)) continue;
        if (!s.earliest_departure || f.dep_time < s.earliest_departure->dep_time)
            s.earliest_departure = f;
        // ... 其余五个极值同理
    }
    return s;
}
```

单遍扫描求六个极值（起飞最早/最晚、飞行最短/最长、票价最低/最高）。用 `std::optional<Flight>` 表示「可能为空」——空数据库时字段为 `nullopt`，而不是用一个不存在的哨兵值。`std::optional` 的 `operator<` 语义（`nullopt` 恒小于任何值）恰好被 `!s.earliest_departure ||` 的短路逻辑显式处理了。

### 3.7 直达标记 — `best_flights()`（实验一）

```cpp
BestFlights best;   // cheapest / shortest_duration / plus_one_day 各是 vector<Flight>
```

实验一要求对「from → to」的直达航班标记三类最优：

- **cheapest**：票价最低
- **shortest duration**：飞行时间最短
- **+1 day**：降落日期是起飞日期的次日（跨天航班）

**为什么三类各自用 `vector` 存**：最优值**可能有并列**——两条航班票价恰好相同、飞行时间恰好相同、或多条跨天航班。用 `vector` 而非单个 `Flight` 是「返回全部并列最优」的设计起点，这一原则贯穿到实验六的 `optimal_routes`（输出全部并列最优方案）。

`best_flights` 先遍历一遍求 `min_fare` / `min_dur`，再遍历一遍收集所有命中最小值的航班，两遍扫描，逻辑清晰。

### 3.8 批量操作 — `batch_apply()`（实验三）

```cpp
void FlightDatabase::batch_apply(const std::string& ops_path) {
    std::ifstream file(ops_path);
    std::string line;
    int line_no = 0;
    while (std::getline(file, line)) {
        ++line_no;
        // 跳过注释(#)与空行
        // 切出操作码，分发到 add / remove / update
        try {
            if (op == "ADD" || op == "A") { ... }
            else if (op == "DEL" || op == "D") { ... }
            else if (op == "MOD" || op == "M") { ... }
            else throw std::invalid_argument("未知操作码: '" + op + "'");
        } catch (const std::exception& e) {
            throw std::runtime_error("批量操作第 " + std::to_string(line_no) + " 行出错: " + e.what());
        }
    }
}
```

批量操作从文件读取，每行一条指令（`ADD`/`DEL`/`MOD`），`#` 开头为注释、空行忽略。**错误处理**是这里的重点：任何一行出错，**抛异常并中止整批操作**（而非跳过该行继续）——这保证批量操作的原子性，避免「半截数据」状态。抛出的异常把行号包进去，方便定位到具体哪一行。

---

## 4. 图算法层：FlightGraph + RouteService（实验三/六）

### 4.1 定位

这是整个项目**算法密度最高**的一层，承载实验三的「最多乘机」和实验六的「连通性 / 最优方案」。核心是把航班之间的关系建模成一张**有向无环图（DAG）**，然后用拓扑序 + 动态规划 + 回溯枚举求解。

### 4.2 换乘图建模 — `FlightGraph`

**核心抽象**：**节点 = 一条航班，有向边 = 可换乘**。

```cpp
// 节点 = 一条活跃航班，有向边 i->j 表示"乘坐航班 i 后可在机场换乘航班 j"，
// 换乘条件：flights_[i].to_airport == flights_[j].from_airport 且
// flights_[j].dep_time >= flights_[i].arr_time + min_connection_minutes。
class FlightGraph {
    std::vector<Flight> flights_;
    std::vector<std::vector<int>> adj_;      // i -> 可换乘的 j
    std::vector<std::vector<int>> radj_;     // 反向邻接（j <- 可换乘到 j 的 i）
    std::unordered_map<int, std::vector<int>> by_from_;  // 起飞机场 -> 航班下标
    std::unordered_map<int, std::vector<int>> by_to_;    // 到达机场 -> 航班下标
    std::unordered_map<int, int> id_to_index_;
};
```

**为什么「以航班为节点」而非「以机场为节点」**：如果以机场为节点，边权只能表示「从一个机场到另一个机场」的模糊概念，无法表达「同一机场之间不同航班的起飞/到达时间差异」。而以航班为节点，边权精确到「具体哪一班 → 哪一班」，时间约束（换乘 ≥ 30 分钟）和费用约束（票价）都落在节点/边上，是这类「时刻表建模」的标准做法。

**为什么是 DAG**：换乘条件要求 `dep_time_j >= arr_time_i + 30 > dep_time_i`，即**沿任何边，时间严格递增**。时间单调递增 ⇒ 图中不存在环 ⇒ 是有向无环图。这个性质是整个图算法层的地基——它允许我们用**拓扑序 + DP** 求最短/最长路径（复杂度 O(V+E)），而不是用更慢的 Dijkstra/Bellman-Ford（环图才需要）。

`build()` 用 `by_from_` 索引加速建边：对每条航班 i，只在其**到达机场**的「从该机场起飞」列表里找后继，避免 O(n²) 的全量两两比较：

```cpp
for (size_t i = 0; i < n; ++i) {
    const auto it = by_from_.find(flights_[i].to_airport);
    if (it == by_from_.end()) continue;
    const long long earliest_dep = flights_[i].arr_time.total_minutes() + min_connection_;
    for (int j : it->second) {
        if (flights_[j].dep_time.total_minutes() >= earliest_dep) {
            adj_[i].push_back(j);
            radj_[j].push_back(i);
        }
    }
}
```

同时构建 `adj_`（正向）和 `radj_`（反向），正向用于 DP/DFS，反向用于「反向可达性」剪枝（见 4.4）。

### 4.3 拓扑序 — 排序而非 Kahn

```cpp
std::vector<int> RouteService::topological_order(const FlightGraph& g) {
    std::vector<int> order(g.flights().size());
    for (size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<int>(i);   // 0..n-1
    }
    std::stable_sort(order.begin(), order.end(), [&g](int a, int b) {
        return g.flights()[a].dep_time.total_minutes() <
               g.flights()[b].dep_time.total_minutes();
    });
    return order;
}
```

这里没有实现经典的 Kahn 算法（入度队列），而是**直接按起飞时间排序**。这是合法的：因为边总是从「早起飞」指向「晚起飞」（`dep_j > dep_i`），所以「按 `dep_time` 升序」天然是一个拓扑序。用 `stable_sort` 而非 `sort` 是为了**并列起飞时间时保持确定性顺序**，使输出结果稳定可复现（测试依赖这一点）。

这个「排序即拓扑序」的简化，正是 DAG 性质的直接红利——省掉了建入度表、维护队列的样板代码。

### 4.4 最优方案 — `optimal_routes()`（实验六）

实验六要求「不限中转次数」求最短飞行时间（**含转机停留**）或最低航费，并输出**全部并列最优**方案。这是本层最精妙的部分，分四步：

**第一步：定义代价函数**。区分「源节点代价」和「边代价」：

```cpp
// 源节点 i 的初始代价（只算第一班）
const auto source_cost = [&](int i) {
    return criterion == Fare ? flights[i].fare : flights[i].duration_minutes();
};
// 沿边 i->j 的增量代价（算上第 j 班）
const auto edge_cost = [&](int i, int j) {
    if (criterion == Fare) return flights[j].fare;
    return flights[j].duration_minutes() + g.wait_minutes(i, j);  // 飞行 + 转机停留
};
```

关键差别在 **duration 维度把转机停留（`wait_minutes = dep_j - arr_i`）计入代价**，而 fare 维度只累加票价。这体现了两个维度语义的不同：最短时间关心「乘客从出发到到达的总耗时」，最低费用只关心「掏了多少钱」。

**第二步：拓扑序 DP 求最短路径**。

```cpp
std::vector<long long> dist(n, kInf);
for (int i : by_from_safe(g, from)) dist[i] = source_cost(i);   // 初始化源
for (int i : order) {
    if (dist[i] >= kInf) continue;
    for (int j : g.adjacency()[i])
        dist[j] = std::min(dist[j], dist[i] + edge_cost(i, j));
}
```

标准的 DAG 最短路松弛：按拓扑序推进，`dist[i]` 表示「到节点 i 的最小代价」。所有从 `from` 机场起飞的航班都是候选起点，到达 `to` 机场的航班都是候选终点，`optimal` 取终点的 `dist` 最小值。

**第三步：反向可达性剪枝**。

```cpp
std::vector<bool> reach_target(n, false);
// 从所有 target 节点出发，沿反向邻接 radj_ 标记"能到达某个 target"的节点
```

这一步很关键。求完 `dist` 后，我们只知道「哪些节点的最短路代价是多少」，但有些节点虽然 `dist` 有限，却**根本无法到达终点**（比如其后续航班全都不飞往 `to`）。如果不剪枝，第四步回溯枚举时会沿着这些「死路」浪费搜索。反向可达性用一次 BFS 沿 `radj_` 从所有终点回溯，标记出「能到达终点」的节点集合。

**第四步：沿最短路边回溯枚举全部最优方案**。

```cpp
std::function<void(int)> dfs = [&](int i) {
    if (flights[i].to_airport == to && dist[i] == optimal) {
        result.push_back(path);          // 到达终点且代价 == 最优，收集一条
        return;
    }
    for (int j : g.adjacency()[i]) {
        if (reach_target[j] && dist[j] == dist[i] + edge_cost(i, j)) {
            path.push_back(flights[j].id);
            dfs(j);
            path.pop_back();
        }
    }
};
```

核心判断是 `dist[j] == dist[i] + edge_cost(i, j)`——边 (i,j) 属于某条最短路径，当且仅当「经过这条边的代价」恰好等于「到 j 的最优代价」。沿这些「最短路上的边」DFS，枚举出所有并列最优方案。

**为什么 `max_flights` 和 `optimal_routes` 都要「枚举全部」**：题目要求「输出全部并列最优」，而不仅仅是「一条最优路径」。这比只求「一个最优值」难一个量级——DP 能给出最优**值**，但要给出**全部**最优**方案**，必须在 DP 之后再做一次回溯枚举。

### 4.5 最多乘机 — `max_flights()`（实验三）

```cpp
std::vector<int> dist(n, -1);   // 最长路径的航班数，-1 = 不可达
dist[start] = 1;
for (int i : order) {
    if (dist[i] < 0) continue;
    for (int j : g.adjacency()[i])
        dist[j] = std::max(dist[j], dist[i] + 1);
}
res.max_count = *std::max_element(dist.begin(), dist.end());
```

与 `optimal_routes` 对称，只是把 `std::min` 换成 `std::max`（求最长路径的航班数），初始化 `dist[start] = 1`（起始航班本身就计 1 次）。然后同样沿 `dist[j] == dist[i] + 1` 的边 DFS 枚举全部最长路线。`dist` 用 `-1` 而非 `kInf` 表示不可达，因为航班数是正整数，`-1` 天然是个合法哨兵。

### 4.6 连通性 — `connectivity()`（实验六）

```cpp
std::function<void(int, int)> dfs = [&](int node, int remaining) {
    const Flight& f = g.flights()[node];
    if (f.to_airport == to) {           // 已到目的地
        if (!arr_window || arr_window->contains(f.arr_time))
            result.push_back(path);
        return;                          // 到达即停，不再换乘
    }
    if (remaining <= 0) return;          // 中转次数用尽
    for (int j : g.adjacency()[node]) {
        path.push_back(g.flights()[j].id);
        dfs(j, remaining - 1);
        path.pop_back();
    }
};
```

连通性用带「剩余中转次数」的 DFS 枚举。`remaining` 从 `max_transfers` 开始，每次换乘减一，减到 0 就不能再换。两个细节：

- **到达即停**：一旦航班落到 `to`，记录方案并 `return`，不再继续换乘（题目的连通性只关心「能否到达」，不关心「到了之后还能不能继续飞」）。
- **时间窗口**：`dep_window` 约束**首段**起飞时间（在入口循环里过滤），`arr_window` 约束**末段**到达时间（在 `to_airport == to` 时过滤）。中间航段不受窗口约束——这与实验语义一致（「限定出发时段」和「限定到达时段」只约束首尾）。

---

## 5. 机场服务层：AirportService（实验四/五）

### 5.1 定位

`AirportService` 承载实验四（机场名搜索、同省推荐）和实验五（繁忙统计）。它持有机场数据，但**不持有航班**——繁忙统计和同省推荐需要航班数据时，通过 `const FlightDatabase& db` 参数传入。这个「只读依赖」的设计保持了单向依赖：`AirportService` 依赖 `FlightDatabase`，反之不成立。

### 5.2 机场名近似搜索 — `similarity()`（实验四）

```cpp
double AirportService::similarity(const std::string& query, const Airport& airport) {
    double result = 0.0;
    if (airport.country == query || airport.province == query || airport.name == query)
        result += 1.0;                       // 精确匹配加成

    // 字符集包含度：|chars(query) ∩ chars(name)| / |chars(query)|
    const double containment = inter / qcount;
    // 连续匹配度：LCS(query, name) / query.size()
    const double continuous = lcs_len / query.size();

    const double w1 = 0.3, w2 = 0.7;         // 包含性 / 连续匹配权重
    result += w1 * containment + w2 * continuous;
    return result;
}
```

相似度由三部分组成，回答「机场名与查询词多像」：

1. **精确匹配加成（+1.0）**：`country`/`province`/`name` 任一项完全等于查询词时加分。这是「命中即强信号」。
2. **字符集包含度（权重 0.3）**：查询词的字母**集合**有多少出现在机场名里。衡量「字母覆盖度」，容忍顺序颠倒（如 `beijng` 打错也能匹配 `Beijing`）。用 `std::array<int, 26>` 做字母集合（只统计 a-z，忽略大小写）。
3. **连续匹配度（权重 0.7）**：最长公共子串长度占查询词的比例。衡量「顺序保持」——连续匹配比散乱包含更强，所以权重更高。

**为什么 0.3 / 0.7**：连续子串匹配（`continuous`）捕捉的是「用户大概率按正确顺序输入了机场名的某个片段」，信号更强；字符集包含（`containment`）只是兜底「字母大致对上了」。因此连续匹配权重压倒包含度。

`search_by_name` 对全部机场计算相似度，`std::sort` 降序后取前 `k`（默认 5）。

### 5.3 最长公共子串 — 滚动数组优化

```cpp
int longest_common_substring(const std::string& a, const std::string& b) {
    const size_t n = a.size(), m = b.size();
    std::vector<int> prev(m + 1, 0), cur(m + 1, 0);
    int best = 0;
    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (tolower(a[i-1]) == tolower(b[j-1])) {
                cur[j] = prev[j - 1] + 1;
                best = std::max(best, cur[j]);
            } else {
                cur[j] = 0;
            }
        }
        std::swap(prev, cur);
        std::fill(cur.begin(), cur.end(), 0);
    }
    return best;
}
```

经典动态规划：`dp[i][j]` = 以 `a[i-1]`、`b[j-1]` 结尾的最长公共**子串**长度（注意是子串不是子序列，不匹配时清零而非取 max）。时间复杂度 O(n·m)，空间用**滚动数组**（`prev`/`cur` 两个一维数组轮流换）压到 O(m)——因为 `dp[i][j]` 只依赖 `dp[i-1][j-1]`。不区分大小写（`tolower`）保证 `Beijing` 能匹配 `beijing`。

### 5.4 同省推荐 — `recommend_same_province()`（实验四）

```cpp
std::unordered_map<std::string, std::vector<int>> by_province_;  // 省份 -> 机场 id
```

`load` 时建立「省份 → 机场 id 列表」索引。推荐时：

1. 找到 `from`、`to` 各自的省份
2. 取出「与出发机场同省的所有机场」集合和「与到达机场同省的所有机场」集合
3. 遍历活跃航班，找出**起飞机场 ∈ 出发省集合 且 到达机场 ∈ 到达省集合**的直达航班

场景是「两机场无直达时，推荐同省其他机场的直达航线」。CLI 在调用前先判断 `direct_flights` 是否为空，Web 端直接暴露接口。

### 5.5 繁忙机场统计 — `busiest_airports()`（实验五）

```cpp
std::unordered_map<int, AirportTraffic> traffic;
for (const auto& f : db.active_flights()) {
    if (!dep_window || dep_window->contains(f.dep_time))
        traffic[f.from_airport].departures += 1;
    if (!arr_window || arr_window->contains(f.arr_time))
        traffic[f.to_airport].arrivals += 1;
}
// 找 max_total，收集所有 == max_total 的机场，按 airport_id 排序
```

单遍扫描，用 `unordered_map<机场id, {departures, arrivals}>` 聚合。支持**可选的起飞/降落时段过滤**（`dep_window` / `arr_window`），窗口为 `nullopt` 时不限制。最后返回**全部并列最繁忙**的机场（`total() == max_total`），按 `airport_id` 升序排序保证输出确定性。

### 5.6 一个非显而易见的 bug：move-from 之后使用

`load` 初版代码顺序是：

```cpp
airports_.push_back(std::move(a));
by_province_[a.province].push_back(a.id);   // ❌ a 已被 move，字段失效
```

`push_back(std::move(a))` 之后，`a` 已处于 moved-from 状态——`a.province` 变成空串、`a.id` 未定义。此时再读 `a.province` / `a.id` 建立索引，会把空串作为 key 写入错误条目，导致 `by_province_` 索引失效。单测 `RecommendSameProvince`（期望 2 条却返回 0 条）暴露了这个问题。

修复是把索引建立**移到 move 之前**：

```cpp
by_province_[a.province].push_back(a.id);   // 必须在 move 之前读取 a 的字段
airports_.push_back(std::move(a));
```

**教训**：容器 `emplace_back`/`push_back` 配合 `std::move` 时，任何依赖原对象字段的后续操作必须放在 move 之前，或改用容器内的新元素（`airports_.back()`）访问。这是「move 语义」类 bug 的典型形态——编译器不会报错，只有测试能抓住。

---

## 6. CLI 前端

### 6.1 定位

`flight-cli` 是终端交互式菜单，13 个功能入口覆盖六大实验全部功能，是这些功能最直接的交互形态。

### 6.2 结构

```cpp
class Cli {
public:
    Cli(std::string flights_path, std::string airports_path);
    int run();   // 菜单循环
private:
    FlightDatabase db_;
    AirportService airports_;
    RouteService routes_;   // routes_(db_)，持有对 db_ 的引用
    // 13 个功能入口：show_stats / query_direct / add_flight / ...
};
```

`Cli` 组合了核心库的三个对象。注意 `routes_` 是 `RouteService routes_(db_)`——**成员初始化顺序**要求 `db_` 在 `routes_` 之前声明（C++ 按声明顺序初始化成员），`RouteService` 持有 `const FlightDatabase&` 引用，不复制数据。

### 6.3 输入辅助函数

```cpp
int read_int(const std::string& prompt);          // 循环直到输入合法整数
DateTime read_datetime(const std::string& prompt); // 尝试 "MM/DD/YYYY HH:MM" 再回退 "MM/DD/YYYY"
std::optional<TimeWindow> read_window(...);        // y/n 交互 + 起止时间
```

这些函数都实现「**输入 → 校验 → 失败重试**」的循环，把非法输入挡在核心库之外——核心库抛异常，CLI 层负责把异常转成友好的重试提示。`read_datetime` 先试带时分的格式，失败回退到纯日期格式（`5/5/2017` 视为 `00:00`），提升容错。

### 6.4 数据路径注入

```cpp
#ifndef FLIGHT_DATA_DIR
#define FLIGHT_DATA_DIR "data"
#endif

int main(int argc, char* argv[]) {
    const std::string flights_path = argc > 1 ? argv[1] : FLIGHT_DATA_DIR "/flights.csv";
    ...
}
```

数据路径通过 **CMake 编译期宏 `FLIGHT_DATA_DIR`** 注入默认值（`CMakeLists.txt` 里 `target_compile_definitions` 设为绝对路径），同时支持命令行参数覆盖。这样「可执行文件放在任意目录运行」都能找到数据，又保留了手动指定数据的灵活性。

---

## 7. Web 前端

### 7.1 定位

`flight-web` 是「网页版」前端，用 cpp-httplib 提供 HTTP 服务 + 静态文件托管，前端页面（HTML/JS/CSS）通过 REST API 调用核心库。它证明了核心库的「无 I/O 耦合」设计——同样的业务逻辑，被 CLI 和 Web 两个完全不同的前端复用。

### 7.2 后端结构

```
web/server.cpp   # 入口：加载数据、挂载静态目录、注册 API、listen
web/api.cpp      # REST 路由 + JSON 序列化
web/api.h        # 接口声明
```

`server.cpp` 的职责最小：加载数据、`set_mount_point("/", static_dir)` 挂载静态目录、调用 `register_api`、`listen`。所有路由都集中在 `api.cpp`。

```cpp
void register_api(httplib::Server& server, FlightDatabase& db,
                  AirportService& airports, RouteService& routes) {
    server.Get("/api/stats", ...);
    server.Get("/api/flights", ...);
    server.Get(R"(/api/flights/(\d+))", ...);   // 正则路由捕获 id
    server.Post("/api/flights", ...);
    server.Put(R"(/api/flights/(\d+))", ...);
    server.Delete(R"(/api/flights/(\d+))", ...);
    // ... 其余端点
}
```

**REST API 一览**：

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/stats` | 实验一统计数据 |
| GET | `/api/flights?from=&to=&limit=` | 直达查询 / 全部活跃航班（可 limit） |
| GET/POST/PUT/DELETE | `/api/flights[/:id]` | 查询 / 新增 / 修改 / 删除航班 |
| POST | `/api/airports/suspend` `/resume` | 机场暂停 / 恢复 |
| GET | `/api/airports/search?q=` | 机场名近似搜索 Top-5 |
| GET | `/api/airports/recommend?from=&to=` | 同省机场推荐 |
| GET | `/api/airports/busiest?dep_start=...` | 最繁忙机场（时段可选） |
| GET | `/api/routes/connect?from=&to=&max_transfers=` | 连通性查询 |
| GET | `/api/routes/optimal?from=&to=&criteria=` | 最优乘机方案 |
| GET | `/api/trips/max-flights?start=` | 最多乘机 |

### 7.3 JSON 序列化 — `to_json` / `flight_from_json`

```cpp
nlohmann::json to_json(const Flight& f) {
    return {{"id", f.id}, {"flight_no", f.flight_no},
            {"dep_time", f.dep_time.to_string()}, ...};
}
```

日期时间以**字符串**（`"MM/DD/YYYY HH:MM"`）进出 JSON，而非时间戳——保持与 CSV 一致的可读格式，前端直接展示。`flight_from_json` 从 JSON 重建 `Flight` 对象，其中 `f.date` 从 `dep_time` 的年月日推导（`DateTime(dep_time.year, dep_time.month, dep_time.day)`），因为前端只提交完整的 `dep_time`，不单独传 `date`。

### 7.4 错误处理范式

每个 handler 的「try/catch + `send_error`」是统一范式：

```cpp
server.Post("/api/flights", [&](const httplib::Request& req, httplib::Response& res) {
    try {
        const auto j = nlohmann::json::parse(req.body);
        db.add(flight_from_json(j));
        send_json(res, to_json(f), 201);
    } catch (const std::exception& e) {
        send_error(res, e.what());   // 400
    }
});
```

核心库抛的异常（`invalid_argument`、`runtime_error`）在这里被捕获，转成 HTTP 400 的 JSON 错误响应。**错误信息直接透传** `e.what()`——核心库的异常信息本就设计为中文、面向用户，无需再封装。

### 7.5 静态前端

`web/static/` 下是纯 HTML + 原生 JS + CSS，无构建步骤、无框架。`index.html` 的每个 `<section>` 对应一个实验功能，`app.js` 用 `fetch` 调 REST API 并把结果渲染到对应的 `<div>` 输出区。前端是「薄壳」——所有逻辑都在后端核心库，前端只做「收集参数 → 调 API → 展示结果」。

---

## 8. 架构全景图

### 8.1 模块协作全景

以一次「最优乘机方案」查询为例，看各模块如何协作（其它查询流程类似，只是图算法环节不同）：

```
RouteService::optimal_routes(from, to, criterion)
    │
    ├─ 1. db_.active_flights()   → 从 FlightDatabase 取活跃航班（自动过滤暂停机场）
    ├─ 2. FlightGraph::build()   → 建换乘图（建边 + 起降机场索引）
    ├─ 3. topological_order()    → 按起飞时间排序得到拓扑序
    ├─ 4. 拓扑序 DP              → 求 dist[]（到各节点的最小代价）
    ├─ 5. 反向可达性 BFS         → 标记「能到达终点」的节点，剪掉死路
    └─ 6. 沿最短路边 DFS         → 回溯枚举全部并列最优方案
```

这条链路体现了核心库的分工：`FlightDatabase` 管状态与过滤，`FlightGraph` 管图建模，`RouteService` 管图算法，彼此通过明确的接口衔接。

### 8.2 关键架构决策

| # | 决策 | 取舍 / 原因 |
|---|------|-----------|
| 1 | **核心结构用 STL**，不手写容器 | 用标准库体现工程能力，聚焦业务逻辑而非造轮子 |
| 2 | **前端选网页版**（非桌面 App） | 网页版零安装、可演示、可截图展示 |
| 3 | **换乘 30 分钟规则统一到实验三/六** | 实验三有明确约定，实验六取一致且现实的规则 |
| 4 | **核心库与前端解耦** | `flight_core` 无 I/O 耦合，CLI/Web 复用，可单测 |
| 5 | **测试从 34 → 45 用例** | 按「间接覆盖 vs 直接覆盖」原则补齐盲区 |

### 8.3 分层依赖关系

```
main_cli.cpp ──┐                    ┌── server.cpp
               │                    │
   include/flight/cli.h            web/api.cpp
               │                    │
               ▼                    ▼
        ┌─────────────────────────────┐
        │    flight_core (静态库)       │
        │  AirportService              │
        │    └─依赖 FlightDatabase     │
        │  RouteService                │
        │    └─依赖 FlightDatabase     │
        │       + FlightGraph          │
        │  FlightDatabase              │
        │    └─依赖 CsvReader/DateTime │
        └─────────────────────────────┘
```

依赖方向严格向下：`AirportService` / `RouteService` 依赖 `FlightDatabase`（只读引用），`FlightDatabase` 依赖基础设施，前端依赖核心库。没有反向依赖，没有循环依赖。

### 8.4 代码量分布

| 模块 | 文件 | 核心职责 |
|------|------|---------|
| 基础设施 | `date_time.cpp` + `csv_reader.cpp` | 日期时间值类型、CSV 读取 |
| 数据层 | `flight_database.cpp` | 加载/统计/CRUD/批量/暂停 |
| 图算法层 | `route_graph.cpp` + `route_service.cpp` | 换乘图、连通性、最优方案、最多乘机 |
| 机场服务 | `airport_service.cpp` | 搜索/推荐/繁忙统计 |
| CLI | `main_cli.cpp` + `cli.h` | 交互菜单 |
| Web | `server.cpp` + `api.cpp` + `static/` | HTTP + REST + 静态页 |
| 测试 | 5 个 `test_*.cpp` | 45 个 GoogleTest 用例 |

---

## 9. 关键设计决策汇总

### 9.1 架构原则

| 原则 | 体现 |
|------|------|
| **核心与前端解耦** | `flight_core` 纯业务、无 I/O，CLI 与 Web 复用同一套逻辑 |
| **统一过滤入口** | 暂停语义通过 `active_flights()` / `is_flight_active()` 单点贯彻 |
| **数据不变量多重守护** | 起降机场相同 / 起飞不早于到达 / 票价为负，在 `parse_row`、`add`、`update` 三处校验 |
| **返回全部并列最优** | `best_flights`、`optimal_routes`、`busiest_airports` 都返回 `vector` 存全部并列 |
| **只读依赖** | `AirportService` / `RouteService` 持 `const FlightDatabase&`，不复制、不改写 |
| **一处定义多处消费** | `parse_row` 被 `load`/`batch_apply`/CLI 复用；`CsvReader` 被两个数据源复用 |

### 9.2 算法决策

| 决策 | 原因 |
|------|------|
| **以航班为节点建模换乘图** | 精确表达「具体哪一班 → 哪一班」的时间/费用约束 |
| **时间单调 ⇒ DAG ⇒ 拓扑序 DP** | 换乘时间严格递增，图无环，可用 O(V+E) 的拓扑 DP 而非 Dijkstra |
| **排序即拓扑序** | 边总从早到晚，按 `dep_time` 排序天然是拓扑序，省 Kahn 样板代码 |
| **DP 求值 + 回溯枚举** | DP 给最优值，回溯沿「最短路上的边」枚举全部方案 |
| **反向可达性剪枝** | 枚举前先标记「能到终点」的节点，避免沿死路搜索 |
| **duration 含转机停留，fare 不含** | 两维度语义不同，代价函数分开定义 |
| **滚动数组求 LCS** | 空间从 O(n·m) 降到 O(m) |

### 9.3 工程实践

| 实践 | 说明 |
|------|------|
| **`std::optional` 表示可能为空** | 空数据库的统计字段为 `nullopt`，不用哨兵值 |
| **swap-and-pop 删除** | `remove` 用末尾元素覆盖 + `pop_back`，O(1) 删除 |
| **`unordered_map` 索引** | id 查航班、机场查索引，均 O(1) |
| **`std::stable_sort` 保证确定性** | 拓扑序并列时间、繁忙机场并列时输出稳定可复现 |
| **异常带上下文** | `batch_apply` 异常带行号、`parse_row` 异常带字段名 |
| **编译期数据路径** | `FLIGHT_DATA_DIR` 宏注入默认路径，可执行文件任意位置可运行 |
| **FetchContent 拉依赖** | GoogleTest / httplib / json 自动拉取，无需预装 |

### 9.4 技术选型

| 场景 | 选型 | 理由 |
|------|------|------|
| 语言 | C++17 | 课程要求 + 现代 C++（`std::optional`、`std::move`、lambda） |
| 容器 | STL | 标准库容器是工程基线，聚焦业务逻辑而非手写容器 |
| 图算法 | DAG 拓扑序 + DP + DFS | 换乘图天然无环，复杂度最优 |
| HTTP | cpp-httplib | header-only、轻量、`FetchContent` 易集成 |
| JSON | nlohmann/json | 事实标准、API 简洁 |
| 测试 | GoogleTest + CTest | 主流、`gtest_discover_tests` 自动发现 |

### 9.5 命名体系

| 类型 | 约定 | 示例 |
|------|------|------|
| 类名 | PascalCase | `FlightDatabase`, `RouteService`, `AirportService` |
| 文件名 | snake_case | `flight_database.cpp`, `route_service.cpp` |
| 函数/属性 | snake_case | `active_flights()`, `best_flights()`, `id_to_index_` |
| 私有成员 | `_` 后缀 | `flights_`, `adj_`, `by_province_` |
| 常量 | `k` 前缀 | `kMinConnectionMinutes`, `kDateFmt`, `kInf` |
| 命名空间 | `flight` | 避免全局命名污染 |

---

## 附录：文件索引

### 核心库（`include/flight/` + `src/`）

| 文件 | 类 / 函数 | 职责 |
|------|----------|------|
| `date_time.h/.cpp` | `DateTime` | 日期时间值类型：解析、比较、总分钟数、次日判断 |
| `csv_reader.h/.cpp` | `CsvReader` | CSV 文本读取（行 → 字段数组） |
| `flight.h` | `Flight` | 航班数据模型（11 字段） |
| `airport.h` | `Airport` | 机场数据模型（id/国家/省/名） |
| `flight_database.h/.cpp` | `FlightDatabase`, `FlightStats`, `BestFlights` | 加载/统计/直达/CRUD/批量/暂停恢复（实验一~三） |
| `route_graph.h/.cpp` | `FlightGraph` | 换乘图：建边、邻接、索引 |
| `route_service.h/.cpp` | `RouteService`, `TimeWindow`, `Criterion` | 连通性、最优方案、最多乘机（实验三/六） |
| `airport_service.h/.cpp` | `AirportService` | 机场名搜索、同省推荐、繁忙统计（实验四/五） |
| `cli.h` | `Cli` | CLI 交互菜单接口 |

### 前端入口

| 文件 | 前端 | 说明 |
|------|------|------|
| `src/main_cli.cpp` | CLI | 终端菜单入口，`flight-cli` |
| `web/server.cpp` | Web 后端 | HTTP 服务入口，`flight-web` |
| `web/api.cpp` / `api.h` | Web 后端 | REST 路由 + JSON 序列化 |
| `web/static/index.html` / `app.js` / `style.css` | Web 前端 | 静态页面、原生 JS 调 API |

### 测试（`tests/`）

| 文件 | 覆盖模块 |
|------|---------|
| `test_date_time.cpp` | `DateTime` 解析/比较/差值/次日 |
| `test_csv_reader.cpp` | `CsvReader` 读取/跳过表头/空行 |
| `test_flight_database.cpp` | 统计/直达/CRUD/批量/暂停恢复 |
| `test_route_service.cpp` | 连通性/最优方案/最多乘机/并列场景 |
| `test_airport_service.cpp` | 机场搜索/同省推荐/繁忙统计 |

### 数据（`data/`）

| 文件 | 说明 |
|------|------|
| `flights.csv` | 2346 条航班（11 字段） |
| `airports.csv` | 79 个机场（id/国家/省/名） |
| `batch_ops.txt` / `batch_ops_del.txt` / `batch_ops_bad.txt` | 批量操作测试数据（含非法用例） |
