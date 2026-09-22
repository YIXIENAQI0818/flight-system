#include "flight/csv_reader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace flight {

CsvReader::Table CsvReader::read(const std::string& path, bool skip_header) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + path);
    }

    Table table;
    std::string line;
    bool first = true;
    while (std::getline(file, line)) {
        if (first && skip_header) {
            first = false;
            continue;
        }
        first = false;
        if (line.empty()) {
            continue;
        }
        std::stringstream ss(line);
        Row row;
        std::string field;
        while (std::getline(ss, field, ',')) {
            row.push_back(field);
        }
        table.push_back(std::move(row));
    }
    return table;
}

} // namespace flight
