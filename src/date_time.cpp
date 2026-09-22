#include "flight/date_time.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace flight {

namespace {

// 从公历日期计算"距 1970-01-01 的天数"（proleptic Gregorian，Howard Hinnant 算法）。
int days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);      // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;     // [0, 146096]
    return era * 146097 + static_cast<int>(doe) - 719468;
}

} // namespace

DateTime DateTime::parse(const std::string& s, const std::string& fmt) {
    std::tm tm{};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, fmt.c_str());
    if (ss.fail()) {
        throw std::runtime_error("无法解析日期时间: '" + s + "' (格式 " + fmt + ")");
    }
    DateTime dt;
    dt.year = tm.tm_year + 1900;
    dt.month = tm.tm_mon + 1;
    dt.day = tm.tm_mday;
    dt.hour = tm.tm_hour;
    dt.minute = tm.tm_min;
    return dt;
}

int DateTime::day_number() const {
    return days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
}

long long DateTime::total_minutes() const {
    return static_cast<long long>(day_number()) * 24 * 60 + hour * 60 + minute;
}

std::string DateTime::to_string() const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << month << '/'
        << std::setw(2) << day << '/'
        << year << ' '
        << std::setw(2) << hour << ':'
        << std::setw(2) << minute;
    return oss.str();
}

int minutes_between(const DateTime& a, const DateTime& b) {
    return static_cast<int>(b.total_minutes() - a.total_minutes());
}

bool is_next_day(const DateTime& a, const DateTime& b) {
    return b.day_number() == a.day_number() + 1;
}

} // namespace flight
