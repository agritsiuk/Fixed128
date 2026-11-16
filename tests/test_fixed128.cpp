#include <gtest/gtest.h>
#include "fixed128/fixed128.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

using Fixed128     = fixed128::Fixed128<127>;
using RoundingMode = fixed128::RoundingMode;

// ===========================================================================
// Constructor
// ===========================================================================

TEST(Fixed128Constructor, StoresMantissaAndExponent) {
    Fixed128 v(150000000LL, 8);
    EXPECT_EQ((int64_t)v.mantissa, 150000000LL);
    EXPECT_EQ(v.exponent, 8);
}

TEST(Fixed128Constructor, AcceptsInt64MinAndMax) {
    Fixed128 hi(std::numeric_limits<int64_t>::max(), 0);
    EXPECT_EQ((int64_t)hi.mantissa, std::numeric_limits<int64_t>::max());
    Fixed128 lo(std::numeric_limits<int64_t>::min(), 0);
    EXPECT_EQ((int64_t)lo.mantissa, std::numeric_limits<int64_t>::min());
}

TEST(Fixed128MaxPrecision, StaticMemberExposed) {
    static_assert(fixed128::Fixed128<127>::max_precision == 127);
    static_assert(fixed128::Fixed128<8>::max_precision == 8);
    static_assert(fixed128::Fixed128<0>::max_precision == 0);
    SUCCEED();
}

// ===========================================================================
// Addition / subtraction
// ===========================================================================

TEST(Fixed128Addition, SameExponent) {
    Fixed128 s = Fixed128(150000000LL, 8) + Fixed128(250000000LL, 8);
    EXPECT_EQ((int64_t)s.mantissa, 400000000LL);
    EXPECT_EQ(s.exponent, 8);
}

TEST(Fixed128Addition, DifferentExponentsUsesMax) {
    Fixed128 s = Fixed128(150000000LL, 8) + Fixed128(1LL, 3);
    EXPECT_EQ((int64_t)s.mantissa, 150100000LL);
    EXPECT_EQ(s.exponent, 8);
}

TEST(Fixed128Addition, NegativeOperands) {
    Fixed128 s = Fixed128(-150000000LL, 8) + Fixed128(250000000LL, 8);
    EXPECT_EQ((int64_t)s.mantissa, 100000000LL);
}

TEST(Fixed128Addition, WithZero) {
    Fixed128 s = Fixed128(150000000LL, 8) + Fixed128(0LL, 8);
    EXPECT_EQ((int64_t)s.mantissa, 150000000LL);
}

TEST(Fixed128Addition, OverflowThrows) {
    Fixed128 big(std::numeric_limits<int64_t>::min(), 0);
    Fixed128 sq = big * big;                             // 2^126
    EXPECT_THROW((void)(sq + sq), std::overflow_error);  // 2^127
}

TEST(Fixed128Subtraction, SameExponent) {
    Fixed128 d = Fixed128(300000000LL, 8) - Fixed128(150000000LL, 8);
    EXPECT_EQ((int64_t)d.mantissa, 150000000LL);
}

TEST(Fixed128Subtraction, ResultNegative) {
    Fixed128 d = Fixed128(150000000LL, 8) - Fixed128(250000000LL, 8);
    EXPECT_EQ((int64_t)d.mantissa, -100000000LL);
}

TEST(Fixed128Subtraction, DifferentExponents) {
    Fixed128 d = Fixed128(200000000LL, 8) - Fixed128(1LL, 3);
    EXPECT_EQ((int64_t)d.mantissa, 199900000LL);
}

TEST(Fixed128Subtraction, EqualsGivesZero) {
    Fixed128 d = Fixed128(150000000LL, 8) - Fixed128(150000000LL, 8);
    EXPECT_EQ((int64_t)d.mantissa, 0LL);
}

TEST(Fixed128Subtraction, OverflowThrows) {
    Fixed128 big(std::numeric_limits<int64_t>::min(), 0);
    Fixed128 sq  = big * big;                     // 2^126
    Fixed128 neg = Fixed128(0LL, 0) - sq;         // -2^126
    EXPECT_THROW((void)(sq - neg), std::overflow_error);
}

// ===========================================================================
// Multiplication
// ===========================================================================

TEST(Fixed128Multiplication, BasicProduct) {
    Fixed128 p = Fixed128(150000000LL, 8) * Fixed128(200000000LL, 8);
    EXPECT_EQ(p.exponent, 16);
    EXPECT_EQ((int64_t)p.mantissa, 30000000000000000LL);
}

TEST(Fixed128Multiplication, NegativeProduct) {
    Fixed128 p = Fixed128(-150000000LL, 8) * Fixed128(200000000LL, 8);
    EXPECT_EQ((int64_t)p.mantissa, -30000000000000000LL);
}

TEST(Fixed128Multiplication, BothNegative) {
    Fixed128 p = Fixed128(-150000000LL, 8) * Fixed128(-200000000LL, 8);
    EXPECT_EQ((int64_t)p.mantissa, 30000000000000000LL);
}

TEST(Fixed128Multiplication, OverflowThrows) {
    Fixed128 big(std::numeric_limits<int64_t>::max(), 0);
    Fixed128 sq = big * big;
    EXPECT_THROW((void)(sq * sq), std::overflow_error);
}

TEST(Fixed128Multiplication, NegativeExponentUnderflowThrows) {
    // natural = -128 + -128 = -256, below int8_t range.
    Fixed128 a(1LL, -128);
    Fixed128 b(1LL, -128);
    EXPECT_THROW((void)(a * b), std::overflow_error);
}

// ===========================================================================
// Division (target = max(ea, eb))
// ===========================================================================

TEST(Fixed128Division, SameExponentKeepsExponent) {
    Fixed128 q = Fixed128(100000000LL, 8) / Fixed128(300000000LL, 8);
    EXPECT_EQ(q.exponent, 8);
    EXPECT_EQ((int64_t)q.mantissa, 33333333LL);
}

TEST(Fixed128Division, HigherRightExponentImprovesPrecision) {
    // num.exp = 1, div.exp = 8. Target = max(1, 8) = 8, result = 0.33333333.
    Fixed128 q = Fixed128(10LL, 1) / Fixed128(300000000LL, 8);
    EXPECT_EQ(q.exponent, 8);
    EXPECT_EQ((int64_t)q.mantissa, 33333333LL);
}

TEST(Fixed128Division, MultiplyThenDivideUsesMaxTarget) {
    Fixed128 x(150000000LL, 8), y(200000000LL, 8), z(300000000LL, 8);
    Fixed128 r = (x * y) / z;   // x*y has exp 16; max(16, 8) = 16
    EXPECT_EQ(r.exponent, 16);
    EXPECT_EQ((int64_t)r.mantissa, 10000000000000000LL);
}

TEST(Fixed128Division, DivideAtExplicitTarget) {
    Fixed128 q = Fixed128(100000000LL, 8).divide_at(
        Fixed128(300000000LL, 8), 16);
    EXPECT_EQ(q.exponent, 16);
    EXPECT_EQ((int64_t)q.mantissa, 3333333333333333LL);
}

TEST(Fixed128Division, DivisionByZeroThrows) {
    Fixed128 num(100LL, 0), zero(0LL, 0);
    EXPECT_THROW((void)(num / zero), std::domain_error);
    EXPECT_THROW((void)num.divide_at(zero, 8), std::domain_error);
}

TEST(Fixed128Division, NegativeTruncatesTowardZero) {
    Fixed128 q = Fixed128(-100LL, 0) / Fixed128(3LL, 0);
    EXPECT_EQ((int64_t)q.mantissa, -33LL);
}

// ===========================================================================
// MaxPrecision cap on * and /
// ===========================================================================

TEST(Fixed128MaxPrecision, MultiplicationCapped) {
    fixed128::Fixed128<8> a(150000000LL, 8), b(200000000LL, 8);
    auto p = a * b;
    EXPECT_EQ(p.exponent, 8);
    // natural exp = 16, capped to 8: 150000000 * 200000000 / 10^8 = 3 * 10^8
    EXPECT_EQ((int64_t)p.mantissa, 300000000LL);
}

TEST(Fixed128MaxPrecision, MultiplicationNotCappedBelow) {
    fixed128::Fixed128<24> a(150000000LL, 8), b(200000000LL, 8);
    auto p = a * b;
    EXPECT_EQ(p.exponent, 16);
    EXPECT_EQ((int64_t)p.mantissa, 30000000000000000LL);
}

TEST(Fixed128MaxPrecision, MultiplicationExactlyAtCap) {
    fixed128::Fixed128<16> a(150000000LL, 8), b(200000000LL, 8);
    auto p = a * b;
    EXPECT_EQ(p.exponent, 16);
    EXPECT_EQ((int64_t)p.mantissa, 30000000000000000LL);
}

TEST(Fixed128MaxPrecision, MultiplyThenDivideScenarioCap8) {
    fixed128::Fixed128<8> x(150000000LL, 8), y(200000000LL, 8),
                          z(300000000LL, 8);
    auto r = (x * y) / z;
    EXPECT_EQ(r.exponent, 8);
    EXPECT_EQ((int64_t)r.mantissa, 100000000LL);   // 1.0
}

TEST(Fixed128MaxPrecision, MultiplyThenDivideScenarioCap16) {
    fixed128::Fixed128<16> x(150000000LL, 8), y(200000000LL, 8),
                           z(300000000LL, 8);
    auto r = (x * y) / z;
    EXPECT_EQ(r.exponent, 16);
    EXPECT_EQ((int64_t)r.mantissa, 10000000000000000LL);  // 1.0
}

TEST(Fixed128MaxPrecision, DivideAtNotCapped) {
    // divide_at is an explicit request: MaxPrecision does not apply.
    fixed128::Fixed128<8> n(100000000LL, 8), d(300000000LL, 8);
    auto r = n.divide_at(d, 16);
    EXPECT_EQ(r.exponent, 16);
    EXPECT_EQ((int64_t)r.mantissa, 3333333333333333LL);
}

// ===========================================================================
// rescale
// ===========================================================================

TEST(Fixed128Rescale, UpKeepsValue) {
    Fixed128 v = Fixed128(15LL, 1).rescale(3);
    EXPECT_EQ((int64_t)v.mantissa, 1500LL);
    EXPECT_EQ(v.exponent, 3);
}

TEST(Fixed128Rescale, DownTruncates) {
    Fixed128 v = Fixed128(1555LL, 3).rescale(1);
    EXPECT_EQ((int64_t)v.mantissa, 15LL);
    EXPECT_EQ(v.exponent, 1);
}

TEST(Fixed128Rescale, SameExponentReturnsSelf) {
    Fixed128 v = Fixed128(12345LL, 4).rescale(4);
    EXPECT_EQ((int64_t)v.mantissa, 12345LL);
    EXPECT_EQ(v.exponent, 4);
}

// ===========================================================================
// Rounding
// ===========================================================================

TEST(Fixed128Rounding, TruncateTowardZero) {
    EXPECT_EQ((int64_t)Fixed128(159, 2).rescale(1,
              RoundingMode::Truncate).mantissa, 15);
    EXPECT_EQ((int64_t)Fixed128(-159, 2).rescale(1,
              RoundingMode::Truncate).mantissa, -15);
}

TEST(Fixed128Rounding, FloorPositiveAndNegative) {
    EXPECT_EQ((int64_t)Fixed128(159, 2).rescale(1,
              RoundingMode::Floor).mantissa, 15);
    EXPECT_EQ((int64_t)Fixed128(-159, 2).rescale(1,
              RoundingMode::Floor).mantissa, -16);
}

TEST(Fixed128Rounding, CeilingPositiveAndNegative) {
    EXPECT_EQ((int64_t)Fixed128(151, 2).rescale(1,
              RoundingMode::Ceiling).mantissa, 16);
    EXPECT_EQ((int64_t)Fixed128(-151, 2).rescale(1,
              RoundingMode::Ceiling).mantissa, -15);
}

TEST(Fixed128Rounding, HalfUpAwayFromZero) {
    EXPECT_EQ((int64_t)Fixed128(155, 2).rescale(1,
              RoundingMode::HalfUp).mantissa, 16);
    EXPECT_EQ((int64_t)Fixed128(-155, 2).rescale(1,
              RoundingMode::HalfUp).mantissa, -16);
}

TEST(Fixed128Rounding, HalfDownTowardZero) {
    EXPECT_EQ((int64_t)Fixed128(155, 2).rescale(1,
              RoundingMode::HalfDown).mantissa, 15);
    EXPECT_EQ((int64_t)Fixed128(-155, 2).rescale(1,
              RoundingMode::HalfDown).mantissa, -15);
}

TEST(Fixed128Rounding, HalfEvenTiesToEven) {
    // 2.5 -> 2, 3.5 -> 4, 4.5 -> 4, 5.5 -> 6
    EXPECT_EQ((int64_t)Fixed128( 250, 2).rescale(0,
              RoundingMode::HalfEven).mantissa,  2);
    EXPECT_EQ((int64_t)Fixed128( 350, 2).rescale(0,
              RoundingMode::HalfEven).mantissa,  4);
    EXPECT_EQ((int64_t)Fixed128( 450, 2).rescale(0,
              RoundingMode::HalfEven).mantissa,  4);
    EXPECT_EQ((int64_t)Fixed128( 550, 2).rescale(0,
              RoundingMode::HalfEven).mantissa,  6);
    // -2.5 -> -2, -3.5 -> -4
    EXPECT_EQ((int64_t)Fixed128(-250, 2).rescale(0,
              RoundingMode::HalfEven).mantissa, -2);
    EXPECT_EQ((int64_t)Fixed128(-350, 2).rescale(0,
              RoundingMode::HalfEven).mantissa, -4);
}

TEST(Fixed128Rounding, HalfEvenNotExactlyHalf) {
    EXPECT_EQ((int64_t)Fixed128(240, 2).rescale(0,
              RoundingMode::HalfEven).mantissa, 2);  // 2.4 -> 2
    EXPECT_EQ((int64_t)Fixed128(260, 2).rescale(0,
              RoundingMode::HalfEven).mantissa, 3);  // 2.6 -> 3
}

TEST(Fixed128Rounding, HalfFloorAndHalfCeiling) {
    EXPECT_EQ((int64_t)Fixed128( 250, 2).rescale(0,
              RoundingMode::HalfFloor).mantissa,  2);
    EXPECT_EQ((int64_t)Fixed128( 250, 2).rescale(0,
              RoundingMode::HalfCeiling).mantissa, 3);
    EXPECT_EQ((int64_t)Fixed128(-250, 2).rescale(0,
              RoundingMode::HalfFloor).mantissa, -3);
    EXPECT_EQ((int64_t)Fixed128(-250, 2).rescale(0,
              RoundingMode::HalfCeiling).mantissa, -2);
}

TEST(Fixed128Rounding, ExactValueNoRounding) {
    Fixed128 v(200, 2);  // 2.00
    for (auto m : {RoundingMode::Truncate, RoundingMode::Floor,
                   RoundingMode::Ceiling, RoundingMode::HalfUp,
                   RoundingMode::HalfDown, RoundingMode::HalfEven,
                   RoundingMode::HalfFloor, RoundingMode::HalfCeiling}) {
        EXPECT_EQ((int64_t)v.rescale(0, m).mantissa, 2)
            << "mode=" << static_cast<int>(m);
    }
}

TEST(Fixed128Rounding, UpscaleNotAffectedByMode) {
    Fixed128 v = Fixed128(15, 1).rescale(3, RoundingMode::HalfEven);
    EXPECT_EQ((int64_t)v.mantissa, 1500);
    EXPECT_EQ(v.exponent, 3);
}

TEST(Fixed128Rounding, ConvenienceRoundHalfEven) {
    EXPECT_EQ((int64_t)Fixed128(250, 2).round_half_even(0).mantissa, 2);
    EXPECT_EQ((int64_t)Fixed128(350, 2).round_half_even(0).mantissa, 4);
}

TEST(Fixed128Rounding, ConvenienceRoundHalfUp) {
    EXPECT_EQ((int64_t)Fixed128( 250, 2).round_half_up(0).mantissa,  3);
    EXPECT_EQ((int64_t)Fixed128(-250, 2).round_half_up(0).mantissa, -3);
}

TEST(Fixed128Rounding, Int64MinMantissaNoUb) {
    Fixed128 v(std::numeric_limits<int64_t>::min(), 0);
    auto r = v.rescale(0, RoundingMode::HalfEven);
    EXPECT_EQ((int64_t)r.mantissa, std::numeric_limits<int64_t>::min());
}

// ===========================================================================
// normalize
// ===========================================================================

TEST(Fixed128Normalize, AlreadyNormalized) {
    Fixed128 n = Fixed128(12345, 4).normalize();
    EXPECT_EQ((int64_t)n.mantissa, 12345);
    EXPECT_EQ(n.exponent, 4);
}

TEST(Fixed128Normalize, ReducesTrailingZeros) {
    Fixed128 n = Fixed128(1500, 3).normalize();   // 1.500 -> 1.5
    EXPECT_EQ((int64_t)n.mantissa, 15);
    EXPECT_EQ(n.exponent, 1);
}

TEST(Fixed128Normalize, StopsAtExpZero) {
    Fixed128 n = Fixed128(1000, 0).normalize();
    EXPECT_EQ((int64_t)n.mantissa, 1000);
    EXPECT_EQ(n.exponent, 0);
}

TEST(Fixed128Normalize, ZeroCanonicalized) {
    Fixed128 n = Fixed128(0, 8).normalize();
    EXPECT_EQ((int64_t)n.mantissa, 0);
    EXPECT_EQ(n.exponent, 0);
}

TEST(Fixed128Normalize, NegativeValue) {
    Fixed128 n = Fixed128(-1500, 3).normalize();
    EXPECT_EQ((int64_t)n.mantissa, -15);
    EXPECT_EQ(n.exponent, 1);
}

TEST(Fixed128Normalize, DownToInteger) {
    Fixed128 n = Fixed128(100, 2).normalize();    // 1.00 -> 1
    EXPECT_EQ((int64_t)n.mantissa, 1);
    EXPECT_EQ(n.exponent, 0);
}

TEST(Fixed128Normalize, DoesNotChangeValue) {
    Fixed128 v(150000000LL, 8);   // 1.5
    Fixed128 n = v.normalize();
    EXPECT_EQ((int64_t)n.mantissa, 15);
    EXPECT_EQ(n.exponent, 1);
}

// ===========================================================================
// Compound assignment
// ===========================================================================

TEST(Fixed128CompoundAssign, PlusEquals) {
    Fixed128 v(150000000LL, 8);
    v += Fixed128(250000000LL, 8);
    EXPECT_EQ((int64_t)v.mantissa, 400000000LL);
}

TEST(Fixed128CompoundAssign, MinusEquals) {
    Fixed128 v(300000000LL, 8);
    v -= Fixed128(150000000LL, 8);
    EXPECT_EQ((int64_t)v.mantissa, 150000000LL);
}

TEST(Fixed128CompoundAssign, TimesEqualsChangesExponent) {
    Fixed128 v(150000000LL, 8);
    v *= Fixed128(200000000LL, 8);
    EXPECT_EQ((int64_t)v.mantissa, 30000000000000000LL);
    EXPECT_EQ(v.exponent, 16);
}

TEST(Fixed128CompoundAssign, DivideEqualsKeepsExponent) {
    Fixed128 v(30000000000000000LL, 16);
    v /= Fixed128(300000000LL, 8);
    EXPECT_EQ((int64_t)v.mantissa, 10000000000000000LL);
    EXPECT_EQ(v.exponent, 16);
}

TEST(Fixed128CompoundAssign, PlusEqualsSelf) {
    Fixed128 v(150000000LL, 8);
    v += v;
    EXPECT_EQ((int64_t)v.mantissa, 300000000LL);
}

TEST(Fixed128CompoundAssign, ChainedOperations) {
    Fixed128 acc(0LL, 8);
    acc += Fixed128(100000000LL, 8);
    acc += Fixed128(200000000LL, 8);
    acc -= Fixed128(50000000LL, 8);
    EXPECT_EQ((int64_t)acc.mantissa, 250000000LL);
}

// ===========================================================================
// to_string
// ===========================================================================

TEST(Fixed128ToString, PositiveValue) {
    EXPECT_EQ(Fixed128(150000000LL, 8).to_string(), "1.50000000");
}
TEST(Fixed128ToString, IntegerValue) {
    EXPECT_EQ(Fixed128(100000000LL, 8).to_string(), "1.00000000");
}
TEST(Fixed128ToString, NegativeValue) {
    EXPECT_EQ(Fixed128(-150000000LL, 8).to_string(), "-1.50000000");
}
TEST(Fixed128ToString, ZeroValue) {
    EXPECT_EQ(Fixed128(0LL, 8).to_string(), "0.00000000");
}
TEST(Fixed128ToString, FractionalOnly) {
    EXPECT_EQ(Fixed128(5LL, 8).to_string(), "0.00000005");
}
TEST(Fixed128ToString, NoExponent) {
    EXPECT_EQ(Fixed128(42LL, 0).to_string(), "42");
}
TEST(Fixed128ToString, Int64MinNoUb) {
    Fixed128 v(std::numeric_limits<int64_t>::min(), 0);
    EXPECT_EQ(v.to_string(), "-9223372036854775808");
}

// ===========================================================================
// Financial scenarios
// ===========================================================================

TEST(Fixed128Finance, CompoundInterest) {
    Fixed128 principal(1234567890123LL, 8);   // 12345.67890123
    Fixed128 rate(550000000LL, 8);            // 5.5
    Fixed128 hundred(10000000000LL, 8);       // 100
    Fixed128 interest = (principal * rate) / hundred;
    EXPECT_EQ(interest.exponent, 16);
    EXPECT_EQ((int64_t)interest.mantissa, 6790123395676500000LL);
}

TEST(Fixed128Finance, IncrementDecrement) {
    Fixed128 b(10000000000LL, 8);             // 100.00
    b += Fixed128(1234000000LL, 8);           // +12.34
    b -= Fixed128(567000000LL, 8);            // -5.67
    EXPECT_EQ((int64_t)b.mantissa, 10667000000LL);
    EXPECT_EQ(b.exponent, 8);
}

TEST(Fixed128Finance, BankersRoundingRealistic) {
    // 2.345 -> 2.34 (HalfEven: 234 is even)
    Fixed128 v(234500, 5);
    Fixed128 r = v.rescale(2, RoundingMode::HalfEven);
    EXPECT_EQ((int64_t)r.mantissa, 234);

    // 2.355 -> 2.36 (HalfEven: 235 is odd, so bump to even 236)
    Fixed128 v2(235500, 5);
    Fixed128 r2 = v2.rescale(2, RoundingMode::HalfEven);
    EXPECT_EQ((int64_t)r2.mantissa, 236);
}
