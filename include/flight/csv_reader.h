#ifndef FLIGHT_CSV_READER_H
#define FLIGHT_CSV_READER_H

#include <string>
#include <vector>

namespace flight {

// 简单的 CSV 读取工具：按行读取，逗号分隔，可选跳过表头。
class CsvReader {
public:
    using Row = std::vector<std::string>;
    using Table = std::vector<Row>;

    // 读取整个 CSV 文件。skip_header 为 true 时丢弃第一行。
    // 空行会被忽略；打开失败抛 std::runtime_error。
    static Table read(const std::string& path, bool skip_header = true);
};

} // namespace flight

#endif // FLIGHT_CSV_READER_H
