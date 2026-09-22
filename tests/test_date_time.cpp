#include "flight/date_time.h"

#include <gtest/gtest.h>

using flight::DateTime;
using flight::is_next_day;
using flight::minutes_between;

TEST(DateTime, ParseDateAndTime) {
    const DateTime d = DateTime::parse("5/5/2017", "%m/%d/%Y");
    EXPECT_EQ(d.year, 2017);
    EXPECT_EQ(d.month, 5);
    EXPECT_EQ(d.day, 5);

    const DateTime t = DateTime::parse("12/31/2017 23:59", "%m/%d/%Y %H:%M");
    EXPECT_EQ(t.year, 2017);
    EXPECT_EQ(t.month, 12);
    EXPECT_EQ(t.day, 31);
    EXPECT_EQ(t.hour, 23);
    EXPECT_EQ(t.minute, 59);
}

TEST(DateTime, ParseSingleDigitMonthDay) {
    const DateTime t = DateTime::parse("5/5/2017 12:20", "%m/%d/%Y %H:%M");
    EXPECT_EQ(t.month, 5);
    EXPECT_EQ(t.day, 5);
    EXPECT_EQ(t.hour, 12);
    EXPECT_EQ(t.minute, 20);
}

TEST(DateTime, Comparison) {
    const DateTime a(2017, 5, 5, 8, 0);
    const DateTime b(2017, 5, 5, 9, 0);
    EXPECT_LT(a, b);
    EXPECT_LE(a, a);
    EXPECT_GT(b, a);
    EXPECT_EQ(a, DateTime(2017, 5, 5, 8, 0));
}

TEST(DateTime, MinutesBetween) {
    const DateTime a(2017, 5, 5, 8, 0);
    const DateTime b(2017, 5, 5, 9, 30);
    EXPECT_EQ(minutes_between(a, b), 90);
    EXPECT_EQ(minutes_between(b, a), -90);
}

TEST(DateTime, MinutesBetweenCrossMidnight) {
    const DateTime a(2017, 5, 5, 23, 0);
    const DateTime b(2017, 5, 6, 1, 0);
    EXPECT_EQ(minutes_between(a, b), 120);
}

TEST(IsNextDay, SameDay) {
    const DateTime a(2017, 5, 5, 8, 0);
    const DateTime b(2017, 5, 5, 23, 0);
    EXPECT_FALSE(is_next_day(a, b));
}

TEST(IsNextDay, OrdinaryNextDay) {
    const DateTime a(2017, 5, 5, 23, 0);
    const DateTime b(2017, 5, 6, 1, 0);
    EXPECT_TRUE(is_next_day(a, b));
}

TEST(IsNextDay, CrossMonth) {
    const DateTime a(2017, 5, 31, 23, 0);
    const DateTime b(2017, 6, 1, 1, 0);
    EXPECT_TRUE(is_next_day(a, b));
}

TEST(IsNextDay, CrossYear) {
    const DateTime a(2017, 12, 31, 23, 0);
    const DateTime b(2018, 1, 1, 1, 0);
    EXPECT_TRUE(is_next_day(a, b));
}

TEST(IsNextDay, TwoDaysGap) {
    const DateTime a(2017, 5, 5, 8, 0);
    const DateTime b(2017, 5, 7, 8, 0);
    EXPECT_FALSE(is_next_day(a, b));
}
