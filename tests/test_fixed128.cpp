#include <gtest/gtest.h>
#include "fixed128/fixed128.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>

using Fixed128     = fixed128::Fixed128<16>;
using RoundingMode = fixed128::RoundingMode;

// ===========================================================================
// Constructors / accessors
// ===========================================================================

TEST(Fixed128Constructor, StoresMantissaAndExponent) {
    Fixed128 v(150000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 150000000LL);
    EXPECT_EQ(v.raw_exponent(), 8);
}

TEST(Fixed128Constructor, FromInt64MinMax) {
    Fixed128 hi(std::numeric_limits<int64_t>::max(), 0);
    EXPECT_EQ(static_cast<int64_t>(hi.raw_mantissa()),
              std::numeric_limits<int64_t>::max());
    Fixed128 lo(std::numeric_limits<int64_t>::min(), 0);
    EXPECT_EQ(static_cast<int64_t>(lo.raw_mantissa()),
              std::numeric_limits<int64_t>::min());
}

TEST(Fixed128Constructor, FromInt128) {
    fixed128::int128_t big = fixed128::int128_t(1) << 60;
    Fixed128 v(big, 0);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()),
              static_cast<int64_t>(int64_t(1) << 60));
    EXPECT_EQ(v.raw_exponent(), 0);
}

TEST(Fixed128Constructor, FromParts) {
    auto v = Fixed128::from_parts(fixed128::int128_t(1234567890123456LL), 12);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 1234567890123456LL);
    EXPECT_EQ(v.raw_exponent(), 12);
}

TEST(Fixed128MaxPrecision, StaticMemberExposed) {
    static_assert(fixed128::Fixed128<127>::max_precision == 127);
    static_assert(fixed128::Fixed128<0>::max_precision == 0);
    SUCCEED();
}

// ===========================================================================
// Unary
// ===========================================================================

TEST(Fixed128Unary, NegationFlipsMantissa) {
    Fixed128 n = -Fixed128(150000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), -150000000LL);
    EXPECT_EQ(n.raw_exponent(), 8);
}
TEST(Fixed128Unary, DoubleNegationIsIdentity) {
    Fixed128 v(150000000LL, 8);
    Fixed128 n = -(-v);
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), 150000000LL);
}

// ===========================================================================
// ++ / --
// ===========================================================================

TEST(Fixed128Increment, PreIncrementAddsOne) {
    Fixed128 v(150000000LL, 8);
    ++v;
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 250000000LL);
}
TEST(Fixed128Increment, PostIncrementReturnsPrevious) {
    Fixed128 v(150000000LL, 8);
    Fixed128 before = v++;
    EXPECT_EQ(static_cast<int64_t>(before.raw_mantissa()), 150000000LL);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()),      250000000LL);
}
TEST(Fixed128Increment, NegativeExponent) {
    Fixed128 v(1LL, -1);
    ++v;
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 11LL);
    EXPECT_EQ(v.raw_exponent(), 0);
}

// ===========================================================================
// Comparison
// ===========================================================================

TEST(Fixed128Comparison, EqualDifferentScales) {
    Fixed128 a(15LL, 1), b(150LL, 2);
    EXPECT_TRUE(a == b);
}
TEST(Fixed128Comparison, NegativeValues) {
    Fixed128 neg(-150000000LL, 8), pos(150000000LL, 8), zero(0LL, 8);
    EXPECT_TRUE(neg < pos); EXPECT_TRUE(neg < zero); EXPECT_TRUE(pos > zero);
}
TEST(Fixed128Comparison, HugeExponentGap) {
    Fixed128 tiny(1LL, 127), huge(1LL, -128);
    EXPECT_TRUE(tiny < huge); EXPECT_TRUE(huge > tiny);
}
TEST(Fixed128Comparison, EqualToInt64) {
    Fixed128 b(1LL, 0);
    EXPECT_TRUE(b == 1LL);
    EXPECT_TRUE(1LL == b);
    EXPECT_TRUE(b != 2LL);
}

// ===========================================================================
// Add / sub
// ===========================================================================

TEST(Fixed128Addition, SameExponent) {
    Fixed128 s = Fixed128(150000000LL, 8) + Fixed128(250000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(s.raw_mantissa()), 400000000LL);
}
TEST(Fixed128Addition, DifferentExponentsUsesMax) {
    Fixed128 s = Fixed128(150000000LL, 8) + Fixed128(1LL, 3);
    EXPECT_EQ(static_cast<int64_t>(s.raw_mantissa()), 150100000LL);
    EXPECT_EQ(s.raw_exponent(), 8);
}
TEST(Fixed128Subtraction, DifferentExponents) {
    Fixed128 d = Fixed128(200000000LL, 8) - Fixed128(1LL, 3);
    EXPECT_EQ(static_cast<int64_t>(d.raw_mantissa()), 199900000LL);
}

// ===========================================================================
// Mul
// ===========================================================================

TEST(Fixed128Multiplication, BasicProduct) {
    Fixed128 p = Fixed128(150000000LL, 8) * Fixed128(200000000LL, 8);
    EXPECT_EQ(p.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()),
              30000000000000000LL);
}
TEST(Fixed128Multiplication, LargeDownShiftReturnsZero) {
    fixed128::Fixed128<0> a(1LL, 127), b(1LL, 127);
    auto p = a * b;
    EXPECT_EQ(p.raw_exponent(), 0);
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), 0LL);
}
TEST(Fixed128Multiplication, ZeroMantissaExtremeExponent) {
    // P1 fix from review: zero * zero with exponents below int8_t range
    // must NOT throw.
    fixed128::Fixed128<0> a(0LL, -100), b(1LL, -100);
    auto p = a * b;
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), 0LL);
}

// ===========================================================================
// Div
// ===========================================================================

TEST(Fixed128Division, TargetIsMaxPrecision) {
    Fixed128 q = Fixed128(100000000LL, 8) / Fixed128(300000000LL, 8);
    EXPECT_EQ(q.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(q.raw_mantissa()), 3333333333333333LL);
}
TEST(Fixed128Division, DivisionByZeroThrows) {
    Fixed128 num(100LL, 0), zero(0LL, 0);
    EXPECT_THROW((void)(num / zero), std::domain_error);
}
TEST(Fixed128Division, NegativeTruncatesTowardZero) {
    Fixed128 q = Fixed128(-100LL, 0) / Fixed128(3LL, 0);
    EXPECT_EQ(q.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(q.raw_mantissa()),
              -333333333333333333LL);
}
TEST(Fixed128Division, ZeroNumeratorExtremeShift) {
    // P1 fix: zero numerator with huge scaling shift must NOT throw.
    Fixed128 num(0LL, 0), den(1LL, 0);
    auto q = num.divide_at(den, 100);  // forces scaling_shift > 38
    EXPECT_EQ(q.raw_exponent(), 100);
    EXPECT_EQ(static_cast<int64_t>(q.raw_mantissa()), 0LL);
}

// ===========================================================================
// MaxPrecision cap
// ===========================================================================

TEST(Fixed128MaxPrecision, MultiplicationCapped) {
    fixed128::Fixed128<8> a(150000000LL, 8), b(200000000LL, 8);
    auto p = a * b;
    EXPECT_EQ(p.raw_exponent(), 8);
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), 300000000LL);
}

// ===========================================================================
// Rescale / rounding
// ===========================================================================

TEST(Fixed128Rescale, UpKeepsValue) {
    Fixed128 v = Fixed128(15LL, 1).rescale(3);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 1500LL);
    EXPECT_EQ(v.raw_exponent(), 3);
}
TEST(Fixed128Rescale, ZeroToLargeExponentNoThrow) {
    Fixed128 v = Fixed128(0LL, 0).rescale(100);
    EXPECT_EQ(v.raw_exponent(), 100);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 0LL);
}

TEST(Fixed128Rounding, Truncate) {
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(159, 2).rescale(1, RoundingMode::Truncate).raw_mantissa()), 15);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(-159, 2).rescale(1, RoundingMode::Truncate).raw_mantissa()), -15);
}
TEST(Fixed128Rounding, Floor) {
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(-159, 2).rescale(1, RoundingMode::Floor).raw_mantissa()), -16);
}
TEST(Fixed128Rounding, Ceiling) {
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(151, 2).rescale(1, RoundingMode::Ceiling).raw_mantissa()), 16);
}
TEST(Fixed128Rounding, AwayFromZero) {
    // NEW: always round away from zero.
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(151, 2).rescale(1, RoundingMode::AwayFromZero).raw_mantissa()), 16);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(-151, 2).rescale(1, RoundingMode::AwayFromZero).raw_mantissa()), -16);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(150, 2).rescale(1, RoundingMode::AwayFromZero).raw_mantissa()), 15);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(-150, 2).rescale(1, RoundingMode::AwayFromZero).raw_mantissa()), -15);
}
TEST(Fixed128Rounding, HalfUp) {
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(155, 2).rescale(1, RoundingMode::HalfUp).raw_mantissa()), 16);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(-155, 2).rescale(1, RoundingMode::HalfUp).raw_mantissa()), -16);
}
TEST(Fixed128Rounding, HalfEven) {
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(250, 2).rescale(0, RoundingMode::HalfEven).raw_mantissa()), 2);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(350, 2).rescale(0, RoundingMode::HalfEven).raw_mantissa()), 4);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(-250, 2).rescale(0, RoundingMode::HalfEven).raw_mantissa()), -2);
}
TEST(Fixed128Rounding, HalfOdd) {
    // NEW: ties to odd.
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(250, 2).rescale(0, RoundingMode::HalfOdd).raw_mantissa()), 3);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(350, 2).rescale(0, RoundingMode::HalfOdd).raw_mantissa()), 3);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(-250, 2).rescale(0, RoundingMode::HalfOdd).raw_mantissa()), -3);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(-350, 2).rescale(0, RoundingMode::HalfOdd).raw_mantissa()), -3);
    // Non-ties fall back to nearest.
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(240, 2).rescale(0, RoundingMode::HalfOdd).raw_mantissa()), 2);
    EXPECT_EQ(static_cast<int64_t>(
        Fixed128(260, 2).rescale(0, RoundingMode::HalfOdd).raw_mantissa()), 3);
}

// ===========================================================================
// normalize / to_string / from_string
// ===========================================================================

TEST(Fixed128Normalize, ReducesTrailingZeros) {
    Fixed128 n = Fixed128(1500, 3).normalize();
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), 15);
    EXPECT_EQ(n.raw_exponent(), 1);
}
TEST(Fixed128ToString, Int64MinNoUb) {
    Fixed128 v(std::numeric_limits<int64_t>::min(), 0);
    EXPECT_EQ(v.to_string(), "-9223372036854775808");
}
TEST(Fixed128FromString, RoundTrip) {
    const char* cases[] = {
        "0", "1", "-1", "123", "-123", "1.5", "-1.5",
        "0.001", "123456.789", "-123456.789",
    };
    for (const char* s : cases) {
        EXPECT_EQ(Fixed128::from_string(s).to_string(), s) << s;
    }
}
TEST(Fixed128FromString, BeyondInt64) {
    const std::string big = "123456789012345678901234567890";
    EXPECT_EQ(Fixed128::from_string(big).to_string(), big);
}
TEST(Fixed128FromString, Errors) {
    EXPECT_THROW(Fixed128::from_string(""),      std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("-"),     std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("1.2.3"), std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("1e5"),   std::invalid_argument);
}

// ===========================================================================
// std::hash
// ===========================================================================

TEST(Fixed128Hash, EqualValuesSameHash) {
    std::hash<Fixed128> h;
    EXPECT_EQ(h(Fixed128(15, 1)),  h(Fixed128(150, 2)));
    EXPECT_EQ(h(Fixed128(1, 0)),   h(Fixed128(100, 2)));
    EXPECT_EQ(h(Fixed128(0, 0)),   h(Fixed128(0, 8)));
    EXPECT_EQ(h(Fixed128(-15, 1)), h(Fixed128(-150, 2)));
}

TEST(Fixed128Hash, UsableInUnorderedSet) {
    std::unordered_set<Fixed128> s;
    s.insert(Fixed128(15, 1));
    s.insert(Fixed128(150, 2));   // equal
    s.insert(Fixed128(1, 0));
    EXPECT_EQ(s.size(), 2u);
    EXPECT_NE(s.find(Fixed128(1, 0)), s.end());
    EXPECT_EQ(s.find(Fixed128(2, 0)), s.end());
}
