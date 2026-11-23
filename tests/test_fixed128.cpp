#include <gtest/gtest.h>
#include "fixed128/fixed128.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

using Fixed128 = fixed128::Fixed128<16>;
using RoundingMode = fixed128::RoundingMode;

// ===========================================================================
// Constructor / accessors
// ===========================================================================

TEST(Fixed128Constructor, StoresMantissaAndExponent) {
    Fixed128 v(150000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 150000000LL);
    EXPECT_EQ(v.raw_exponent(), 8);
}

TEST(Fixed128Constructor, AcceptsInt64MinAndMax) {
    Fixed128 hi(std::numeric_limits<int64_t>::max(), 0);
    EXPECT_EQ(static_cast<int64_t>(hi.raw_mantissa()), std::numeric_limits<int64_t>::max());
    Fixed128 lo(std::numeric_limits<int64_t>::min(), 0);
    EXPECT_EQ(static_cast<int64_t>(lo.raw_mantissa()), std::numeric_limits<int64_t>::min());
}

TEST(Fixed128Constructor, FromParts) {
    auto v = Fixed128::from_parts(fixed128::int128_t(1234567890123456LL), 12);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 1234567890123456LL);
    EXPECT_EQ(v.raw_exponent(), 12);
}

TEST(Fixed128MaxPrecision, StaticMemberExposed) {
    static_assert(fixed128::Fixed128<127>::max_precision == 127);
    static_assert(fixed128::Fixed128<8>::max_precision == 8);
    static_assert(fixed128::Fixed128<0>::max_precision == 0);
    SUCCEED();
}

// ===========================================================================
// Unary operators
// ===========================================================================

TEST(Fixed128Unary, NegationFlipsMantissa) {
    Fixed128 n = -Fixed128(150000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), -150000000LL);
    EXPECT_EQ(n.raw_exponent(), 8);
}
TEST(Fixed128Unary, NegationOfNegativeGivesPositive) {
    Fixed128 n = -Fixed128(-150000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), 150000000LL);
}
TEST(Fixed128Unary, DoubleNegationIsIdentity) {
    Fixed128 v(150000000LL, 8);
    Fixed128 n = -(-v);
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), 150000000LL);
    EXPECT_EQ(n.raw_exponent(), 8);
}
TEST(Fixed128Unary, NegationOfZero) {
    Fixed128 n = -Fixed128(0LL, 8);
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), 0LL);
}
TEST(Fixed128Unary, UnaryPlusIsIdentity) {
    Fixed128 v(150000000LL, 8);
    Fixed128 p = +v;
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), 150000000LL);
    EXPECT_EQ(p.raw_exponent(), 8);
}

// ===========================================================================
// Increment / decrement
// ===========================================================================

TEST(Fixed128Increment, PreIncrementAddsOne) {
    Fixed128 v(150000000LL, 8);
    Fixed128& r = ++v;
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 250000000LL);
    EXPECT_EQ(&r, &v);
}
TEST(Fixed128Increment, PreDecrementSubtractsOne) {
    Fixed128 v(150000000LL, 8);
    --v;
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 50000000LL);
}
TEST(Fixed128Increment, PostIncrementReturnsPrevious) {
    Fixed128 v(150000000LL, 8);
    Fixed128 before = v++;
    EXPECT_EQ(static_cast<int64_t>(before.raw_mantissa()), 150000000LL);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 250000000LL);
}
TEST(Fixed128Increment, PostDecrementReturnsPrevious) {
    Fixed128 v(150000000LL, 8);
    Fixed128 before = v--;
    EXPECT_EQ(static_cast<int64_t>(before.raw_mantissa()), 150000000LL);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 50000000LL);
}
TEST(Fixed128Increment, IncrementZero) {
    Fixed128 v(0LL, 0);
    ++v;
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 1LL);
}
TEST(Fixed128Increment, IncrementNegative) {
    Fixed128 v(-250000000LL, 8);
    ++v;
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), -150000000LL);
}
TEST(Fixed128Increment, IncrementNegativeExponent) {
    Fixed128 v(1LL, -1);
    ++v;
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 11LL);
    EXPECT_EQ(v.raw_exponent(), 0);
}

// ===========================================================================
// Comparison
// ===========================================================================

TEST(Fixed128Comparison, EqualSameRepresentation) {
    Fixed128 a(150000000LL, 8), b(150000000LL, 8);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}
TEST(Fixed128Comparison, EqualDifferentScales) {
    Fixed128 a(15LL, 1), b(150LL, 2), c(1500LL, 3);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(b == c);
    EXPECT_TRUE(a == c);
}
TEST(Fixed128Comparison, EqualIntegerAndDecimal) {
    Fixed128 a(1LL, 0), b(10LL, 1), c(100LL, 2);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(b == c);
}
TEST(Fixed128Comparison, NotEqual) {
    Fixed128 a(15LL, 1), b(16LL, 1);
    EXPECT_TRUE(a != b);
    EXPECT_FALSE(a == b);
}
TEST(Fixed128Comparison, LessAndGreaterSameExponent) {
    Fixed128 small(140000000LL, 8), large(150000000LL, 8);
    EXPECT_TRUE(small < large);
    EXPECT_TRUE(small <= large);
    EXPECT_TRUE(large > small);
    EXPECT_TRUE(large >= small);
    EXPECT_FALSE(large < small);
    EXPECT_FALSE(small > large);
}
TEST(Fixed128Comparison, LessAndGreaterDifferentExponent) {
    Fixed128 small(1LL, 0), large(15LL, 1);
    EXPECT_TRUE(small < large);
    EXPECT_TRUE(large > small);
}
TEST(Fixed128Comparison, NegativeValues) {
    Fixed128 neg(-150000000LL, 8), pos(150000000LL, 8), zero(0LL, 8);
    EXPECT_TRUE(neg < pos);
    EXPECT_TRUE(neg < zero);
    EXPECT_TRUE(pos > zero);
    EXPECT_TRUE(zero > neg);
}
TEST(Fixed128Comparison, ZeroAcrossExponents) {
    Fixed128 z1(0LL, 0), z2(0LL, 8), z3(0LL, -5);
    EXPECT_TRUE(z1 == z2);
    EXPECT_TRUE(z2 == z3);
}
TEST(Fixed128Comparison, DifferentSignWithDifferentScales) {
    Fixed128 neg(-1LL, 0), pos(10LL, 1);
    EXPECT_TRUE(neg < pos);
    EXPECT_TRUE(pos > neg);
}
TEST(Fixed128Comparison, PositiveAndNegativeExponents) {
    Fixed128 a(100LL, 0), b(1LL, -1), c(10LL, -1);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a == c);
}
TEST(Fixed128Comparison, HugeExponentGap) {
    Fixed128 tiny(1LL, 127), huge(1LL, -128);
    EXPECT_TRUE(tiny < huge);
    EXPECT_TRUE(huge > tiny);
}
TEST(Fixed128Comparison, EqualToInt64) {
    Fixed128 a(150000000LL, 8);  // 1.5
    EXPECT_FALSE(a == 1LL);
    EXPECT_TRUE(a != 1LL);
    Fixed128 b(1LL, 0);  // 1
    EXPECT_TRUE(b == 1LL);
    EXPECT_TRUE(1LL == b);
    EXPECT_FALSE(b != 1LL);
}

// ===========================================================================
// Addition / subtraction
// ===========================================================================

TEST(Fixed128Addition, SameExponent) {
    Fixed128 s = Fixed128(150000000LL, 8) + Fixed128(250000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(s.raw_mantissa()), 400000000LL);
    EXPECT_EQ(s.raw_exponent(), 8);
}
TEST(Fixed128Addition, DifferentExponentsUsesMax) {
    Fixed128 s = Fixed128(150000000LL, 8) + Fixed128(1LL, 3);
    EXPECT_EQ(static_cast<int64_t>(s.raw_mantissa()), 150100000LL);
    EXPECT_EQ(s.raw_exponent(), 8);
}
TEST(Fixed128Addition, NegativeOperands) {
    Fixed128 s = Fixed128(-150000000LL, 8) + Fixed128(250000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(s.raw_mantissa()), 100000000LL);
}
TEST(Fixed128Addition, WithZero) {
    Fixed128 s = Fixed128(150000000LL, 8) + Fixed128(0LL, 8);
    EXPECT_EQ(static_cast<int64_t>(s.raw_mantissa()), 150000000LL);
}
TEST(Fixed128Addition, OverflowThrows) {
    Fixed128 big(std::numeric_limits<int64_t>::min(), 0);
    Fixed128 sq = big * big;
    EXPECT_THROW((void)(sq + sq), std::overflow_error);
}
TEST(Fixed128Subtraction, SameExponent) {
    Fixed128 d = Fixed128(300000000LL, 8) - Fixed128(150000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(d.raw_mantissa()), 150000000LL);
}
TEST(Fixed128Subtraction, ResultNegative) {
    Fixed128 d = Fixed128(150000000LL, 8) - Fixed128(250000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(d.raw_mantissa()), -100000000LL);
}
TEST(Fixed128Subtraction, DifferentExponents) {
    Fixed128 d = Fixed128(200000000LL, 8) - Fixed128(1LL, 3);
    EXPECT_EQ(static_cast<int64_t>(d.raw_mantissa()), 199900000LL);
}
TEST(Fixed128Subtraction, OverflowThrows) {
    Fixed128 big(std::numeric_limits<int64_t>::min(), 0);
    Fixed128 sq = big * big;
    Fixed128 neg = Fixed128(0LL, 0) - sq;
    EXPECT_THROW((void)(sq - neg), std::overflow_error);
}

// ===========================================================================
// Multiplication
// ===========================================================================

TEST(Fixed128Multiplication, BasicProduct) {
    Fixed128 p = Fixed128(150000000LL, 8) * Fixed128(200000000LL, 8);
    EXPECT_EQ(p.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), 30000000000000000LL);
}
TEST(Fixed128Multiplication, NegativeProduct) {
    Fixed128 p = Fixed128(-150000000LL, 8) * Fixed128(200000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), -30000000000000000LL);
}
TEST(Fixed128Multiplication, BothNegative) {
    Fixed128 p = Fixed128(-150000000LL, 8) * Fixed128(-200000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), 30000000000000000LL);
}
TEST(Fixed128Multiplication, OverflowThrows) {
    Fixed128 big(std::numeric_limits<int64_t>::max(), 0);
    Fixed128 sq = big * big;
    EXPECT_THROW((void)(sq * sq), std::overflow_error);
}
TEST(Fixed128Multiplication, NegativeExponentUnderflowThrows) {
    Fixed128 a(1LL, -128), b(1LL, -128);
    EXPECT_THROW((void)(a * b), std::overflow_error);
}
TEST(Fixed128Multiplication, LargeDownShiftReturnsZero) {
    // natural = 127 + 127 = 254; MaxPrecision = 0 ⇒ down_shift = 254 > 38.
    // |product| = 1 < 10^254, so the truncated result is 0 (not a throw).
    fixed128::Fixed128<0> a(1LL, 127), b(1LL, 127);
    auto p = a * b;
    EXPECT_EQ(p.raw_exponent(), 0);
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), 0LL);
}

// ===========================================================================
// Division
// ===========================================================================

TEST(Fixed128Division, TargetIsMaxPrecision) {
    Fixed128 q = Fixed128(100000000LL, 8) / Fixed128(300000000LL, 8);
    EXPECT_EQ(q.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(q.raw_mantissa()), 3333333333333333LL);
}
TEST(Fixed128Division, TargetMaxPrecisionIndependentOfInputExponents) {
    Fixed128 q = Fixed128(10LL, 1) / Fixed128(300000000LL, 8);
    EXPECT_EQ(q.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(q.raw_mantissa()), 3333333333333333LL);
}
TEST(Fixed128Division, LowMaxPrecisionTemplate) {
    fixed128::Fixed128<4> a(5, 1), b(2, 1);
    auto q = a / b;
    EXPECT_EQ(q.raw_exponent(), 4);
    EXPECT_EQ(static_cast<int64_t>(q.raw_mantissa()), 25000LL);
}
TEST(Fixed128Division, MultiplyThenDivide) {
    Fixed128 x(150000000LL, 8), y(200000000LL, 8), z(300000000LL, 8);
    Fixed128 r = (x * y) / z;
    EXPECT_EQ(r.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(r.raw_mantissa()), 10000000000000000LL);
}
TEST(Fixed128Division, DivideAtExplicitTarget) {
    Fixed128 q = Fixed128(100000000LL, 8).divide_at(Fixed128(300000000LL, 8), 16);
    EXPECT_EQ(q.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(q.raw_mantissa()), 3333333333333333LL);
}
TEST(Fixed128Division, DivisionByZeroThrows) {
    Fixed128 num(100LL, 0), zero(0LL, 0);
    EXPECT_THROW((void)(num / zero), std::domain_error);
    EXPECT_THROW((void)num.divide_at(zero, 8), std::domain_error);
}
TEST(Fixed128Division, NegativeTruncatesTowardZero) {
    Fixed128 q = Fixed128(-100LL, 0) / Fixed128(3LL, 0);
    EXPECT_EQ(q.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(q.raw_mantissa()), -333333333333333333LL);
}
TEST(Fixed128Division, LargeNegativeShiftReturnsZero) {
    // divide_at(target=-127) forces scaling_shift = -127 - ea + eb.
    // denominator·10^k overflows int128 ⇒ result must be 0 (not a throw).
    Fixed128 num(1LL, 0);
    Fixed128 den(1LL, 100);
    auto q = num.divide_at(den, -127);
    EXPECT_EQ(q.raw_exponent(), -127);
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
TEST(Fixed128MaxPrecision, MultiplicationNotCappedBelow) {
    fixed128::Fixed128<24> a(150000000LL, 8), b(200000000LL, 8);
    auto p = a * b;
    EXPECT_EQ(p.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(p.raw_mantissa()), 30000000000000000LL);
}
TEST(Fixed128MaxPrecision, MultiplyThenDivideScenarioCap8) {
    fixed128::Fixed128<8> x(150000000LL, 8), y(200000000LL, 8), z(300000000LL, 8);
    auto r = (x * y) / z;
    EXPECT_EQ(r.raw_exponent(), 8);
    EXPECT_EQ(static_cast<int64_t>(r.raw_mantissa()), 100000000LL);
}
TEST(Fixed128MaxPrecision, MultiplyThenDivideScenarioCap16) {
    fixed128::Fixed128<16> x(150000000LL, 8), y(200000000LL, 8), z(300000000LL, 8);
    auto r = (x * y) / z;
    EXPECT_EQ(r.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(r.raw_mantissa()), 10000000000000000LL);
}

// ===========================================================================
// Rescale
// ===========================================================================

TEST(Fixed128Rescale, UpKeepsValue) {
    Fixed128 v = Fixed128(15LL, 1).rescale(3);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 1500LL);
    EXPECT_EQ(v.raw_exponent(), 3);
}
TEST(Fixed128Rescale, DownTruncates) {
    Fixed128 v = Fixed128(1555LL, 3).rescale(1);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 15LL);
    EXPECT_EQ(v.raw_exponent(), 1);
}
TEST(Fixed128Rescale, ZeroToLargeExponentNoThrow) {
    Fixed128 v = Fixed128(0LL, 0).rescale(100);
    EXPECT_EQ(v.raw_exponent(), 100);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 0LL);
}

// ===========================================================================
// Rounding
// ===========================================================================

TEST(Fixed128Rounding, TruncateTowardZero) {
    EXPECT_EQ(static_cast<int64_t>(Fixed128(159, 2).rescale(1, RoundingMode::Truncate).raw_mantissa()), 15);
    EXPECT_EQ(static_cast<int64_t>(Fixed128(-159, 2).rescale(1, RoundingMode::Truncate).raw_mantissa()), -15);
}
TEST(Fixed128Rounding, FloorPositiveAndNegative) {
    EXPECT_EQ(static_cast<int64_t>(Fixed128(159, 2).rescale(1, RoundingMode::Floor).raw_mantissa()), 15);
    EXPECT_EQ(static_cast<int64_t>(Fixed128(-159, 2).rescale(1, RoundingMode::Floor).raw_mantissa()), -16);
}
TEST(Fixed128Rounding, CeilingPositiveAndNegative) {
    EXPECT_EQ(static_cast<int64_t>(Fixed128(151, 2).rescale(1, RoundingMode::Ceiling).raw_mantissa()), 16);
    EXPECT_EQ(static_cast<int64_t>(Fixed128(-151, 2).rescale(1, RoundingMode::Ceiling).raw_mantissa()), -15);
}
TEST(Fixed128Rounding, HalfUpAwayFromZero) {
    EXPECT_EQ(static_cast<int64_t>(Fixed128(155, 2).rescale(1, RoundingMode::HalfUp).raw_mantissa()), 16);
    EXPECT_EQ(static_cast<int64_t>(Fixed128(-155, 2).rescale(1, RoundingMode::HalfUp).raw_mantissa()), -16);
}
TEST(Fixed128Rounding, HalfEvenTiesToEven) {
    EXPECT_EQ(static_cast<int64_t>(Fixed128(250, 2).rescale(0, RoundingMode::HalfEven).raw_mantissa()), 2);
    EXPECT_EQ(static_cast<int64_t>(Fixed128(350, 2).rescale(0, RoundingMode::HalfEven).raw_mantissa()), 4);
    EXPECT_EQ(static_cast<int64_t>(Fixed128(-250, 2).rescale(0, RoundingMode::HalfEven).raw_mantissa()), -2);
    EXPECT_EQ(static_cast<int64_t>(Fixed128(-350, 2).rescale(0, RoundingMode::HalfEven).raw_mantissa()), -4);
}

// ===========================================================================
// normalize
// ===========================================================================

TEST(Fixed128Normalize, ReducesTrailingZeros) {
    Fixed128 n = Fixed128(1500, 3).normalize();
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), 15);
    EXPECT_EQ(n.raw_exponent(), 1);
}
TEST(Fixed128Normalize, ZeroCanonicalized) {
    Fixed128 n = Fixed128(0, 8).normalize();
    EXPECT_EQ(static_cast<int64_t>(n.raw_mantissa()), 0);
    EXPECT_EQ(n.raw_exponent(), 0);
}

// ===========================================================================
// Compound assignment
// ===========================================================================

TEST(Fixed128CompoundAssign, PlusEquals) {
    Fixed128 v(150000000LL, 8);
    v += Fixed128(250000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 400000000LL);
}
TEST(Fixed128CompoundAssign, PlusEqualsInt64) {
    Fixed128 v(150000000LL, 8);  // 1.5
    v += 2;                      // +2 → 3.5
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 350000000LL);
    EXPECT_EQ(v.raw_exponent(), 8);
}
TEST(Fixed128CompoundAssign, MinusEqualsInt64) {
    Fixed128 v(150000000LL, 8);  // 1.5
    v -= 1;                      // −1 → 0.5
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 50000000LL);
}
TEST(Fixed128CompoundAssign, TimesEqualsChangesExponent) {
    Fixed128 v(150000000LL, 8);
    v *= Fixed128(200000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 30000000000000000LL);
    EXPECT_EQ(v.raw_exponent(), 16);
}
TEST(Fixed128CompoundAssign, DivideEqualsTargetsMaxPrecision) {
    Fixed128 v(30000000000000000LL, 16);
    v /= Fixed128(300000000LL, 8);
    EXPECT_EQ(static_cast<int64_t>(v.raw_mantissa()), 10000000000000000LL);
    EXPECT_EQ(v.raw_exponent(), 16);
}

// ===========================================================================
// to_string
// ===========================================================================

TEST(Fixed128ToString, PositiveValue) {
    EXPECT_EQ(Fixed128(150000000LL, 8).to_string(), "1.50000000");
}
TEST(Fixed128ToString, NegativeValue) {
    EXPECT_EQ(Fixed128(-150000000LL, 8).to_string(), "-1.50000000");
}
TEST(Fixed128ToString, ZeroValue) {
    EXPECT_EQ(Fixed128(0LL, 8).to_string(), "0.00000000");
}
TEST(Fixed128ToString, Int64MinNoUb) {
    Fixed128 v(std::numeric_limits<int64_t>::min(), 0);
    EXPECT_EQ(v.to_string(), "-9223372036854775808");
}

// ===========================================================================
// Financial scenarios
// ===========================================================================

TEST(Fixed128Finance, CompoundInterest) {
    Fixed128 principal(1234567890123LL, 8);
    Fixed128 rate(550000000LL, 8);
    Fixed128 hundred(10000000000LL, 8);
    Fixed128 interest = (principal * rate) / hundred;
    EXPECT_EQ(interest.raw_exponent(), 16);
    EXPECT_EQ(static_cast<int64_t>(interest.raw_mantissa()), 6790123395676500000LL);
}
TEST(Fixed128Finance, BankersRoundingRealistic) {
    Fixed128 v(234500, 5);
    Fixed128 r = v.rescale(2, RoundingMode::HalfEven);
    EXPECT_EQ(static_cast<int64_t>(r.raw_mantissa()), 234);

    Fixed128 v2(235500, 5);
    Fixed128 r2 = v2.rescale(2, RoundingMode::HalfEven);
    EXPECT_EQ(static_cast<int64_t>(r2.raw_mantissa()), 236);
}
TEST(Fixed128Finance, ComparisonInLedgerCheck) {
    // 150.0000000000000000 at exponent 16
    Fixed128 balance(1500000000000000000LL, 16);
    // 150.00 at exponent 2
    Fixed128 expected(15000LL, 2);
    EXPECT_TRUE(balance == expected);
    EXPECT_FALSE(balance < expected);
    EXPECT_FALSE(balance > expected);
}
