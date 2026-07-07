#include <gtest/gtest.h>

#include "nexus/utility/types/numeric_overflow_detector.h"

using namespace nexus::utility::types;

TEST(NumericOverflowTest, SafeIntBasicOperations) {
    SafeInt<int> a = 100;
    SafeInt<int> b = 200;

    auto c = a + b;
    EXPECT_EQ(c.value(), 300);

    auto d = b - a;
    EXPECT_EQ(d.value(), 100);

    auto e = a * 2;
    EXPECT_EQ(e.value(), 200);
}

TEST(NumericOverflowTest, SafeIntAddOverflow) {
    SafeInt<int> max_val = std::numeric_limits<int>::max();
    EXPECT_THROW({ auto overflow = max_val + 1; (void)overflow; }, ArithmeticOverflowException);
}

TEST(NumericOverflowTest, SafeIntSubOverflow) {
    SafeInt<int> min_val = std::numeric_limits<int>::min();
    EXPECT_THROW({ auto underflow = min_val - 1; (void)underflow; }, ArithmeticOverflowException);
}

TEST(NumericOverflowTest, SafeIntMulOverflow) {
    SafeInt<int> big = std::numeric_limits<int>::max() / 2 + 1;
    EXPECT_THROW({ auto overflow = big * 2; (void)overflow; }, ArithmeticOverflowException);
}

TEST(NumericOverflowTest, NumericOverflowDetectorAdd) {
    auto result = NumericOverflowDetector::add(10, 20);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 30);

    auto overflow_result = NumericOverflowDetector::add(std::numeric_limits<int>::max(), 1);
    EXPECT_FALSE(overflow_result.has_value());
}

TEST(NumericOverflowTest, NumericOverflowDetectorMul) {
    auto result = NumericOverflowDetector::mul(5, 6);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 30);

    auto overflow_result = NumericOverflowDetector::mul(std::numeric_limits<int>::max(), 2);
    EXPECT_FALSE(overflow_result.has_value());
}

TEST(NumericOverflowTest, SafeIntComparison) {
    SafeInt<int> a = 10;
    SafeInt<int> b = 20;
    SafeInt<int> c = 10;

    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
    EXPECT_EQ(a, c);
    EXPECT_NE(a, b);
}
