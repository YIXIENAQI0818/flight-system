#ifndef FLIGHT_DATE_TIME_H
#define FLIGHT_DATE_TIME_H

#include <string>

namespace flight {

// 日期时间值类型：年月日 + 时分。
// 设计为可比较、可测试的纯值类型，解析基于 std::get_time（见实现），
// 内部用"距 1970-01-01 00:00 的总分钟数"做比较与差值运算，规避 std::tm 无法比较、
// 时区相关陷阱。
class DateTime {
public:
    int year = 0;
    int month = 0;  // 1..12
    int day = 0;    // 1..31
    int hour = 0;   // 0..23
    int minute = 0; // 0..59

    DateTime() = default;
    DateTime(int y, int mo, int d, int h = 0, int mi = 0)
        : year(y), month(mo), day(d), hour(h), minute(mi) {}

    // 从字符串解析，fmt 形如 "%m/%d/%Y" 或 "%m/%d/%Y %H:%M"。
    // 解析失败抛出 std::runtime_error。
    static DateTime parse(const std::string& s, const std::string& fmt);

    // 距 1970-01-01 00:00 的总分钟数（可为负）。
    long long total_minutes() const;

    // 格式化为 "MM/DD/YYYY HH:MM"（时分无前导零时补零）。
    std::string to_string() const;

    // 仅比较日期部分，用于相邻日期判断等场景。
    int day_number() const;

    bool operator==(const DateTime& o) const { return total_minutes() == o.total_minutes(); }
    bool operator!=(const DateTime& o) const { return !(*this == o); }
    bool operator<(const DateTime& o) const { return total_minutes() < o.total_minutes(); }
    bool operator<=(const DateTime& o) const { return total_minutes() <= o.total_minutes(); }
    bool operator>(const DateTime& o) const { return total_minutes() > o.total_minutes(); }
    bool operator>=(const DateTime& o) const { return total_minutes() >= o.total_minutes(); }
};

// b - a 的分钟差（有符号）。
int minutes_between(const DateTime& a, const DateTime& b);

// b 的日历日期是否为 a 日历日期的"次日"（正确处理跨月/跨年）。
bool is_next_day(const DateTime& a, const DateTime& b);

} // namespace flight

#endif // FLIGHT_DATE_TIME_H
