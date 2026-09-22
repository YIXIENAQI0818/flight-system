#include "flight/csv_reader.h"

#include <gtest/gtest.h>

#include <fstream>

using flight::CsvReader;

namespace {

void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

} // namespace

TEST(CsvReader, SkipHeader) {
    const std::string path = "/tmp/fs_test_csv.csv";
    write_file(path, "a,b,c\n1,2,3\n4,5,6\n");
    const auto table = CsvReader::read(path, true);
    ASSERT_EQ(table.size(), 2u);
    ASSERT_EQ(table[0].size(), 3u);
    EXPECT_EQ(table[0][0], "1");
    EXPECT_EQ(table[1][2], "6");
}

TEST(CsvReader, KeepHeader) {
    const std::string path = "/tmp/fs_test_csv2.csv";
    write_file(path, "a,b\n1,2\n");
    const auto table = CsvReader::read(path, false);
    ASSERT_EQ(table.size(), 2u);
    EXPECT_EQ(table[0][0], "a");
}

TEST(CsvReader, IgnoresEmptyLines) {
    const std::string path = "/tmp/fs_test_csv3.csv";
    write_file(path, "a,b\n\n1,2\n\n");
    const auto table = CsvReader::read(path, true);
    ASSERT_EQ(table.size(), 1u);
    EXPECT_EQ(table[0][0], "1");
}

TEST(CsvReader, MissingFileThrows) {
    EXPECT_THROW(CsvReader::read("/tmp/fs_no_such_file_xyz.csv", true), std::runtime_error);
}
