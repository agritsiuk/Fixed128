#include <gtest/gtest.h>
#include "fixed128/fixed128_lite.hpp"

#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

using Fixed128 = fixed128::Fixed128<16>;

// ---- constructor ----------------------------------------------------------

TEST(Fixed128LiteConstructor, FromInt64) {
    Fixed128 v(150000000LL, 8);
    EXPECT_EQ(v.raw_exponent(), 8);
    EXPECT_EQ(v.to_string(), "1.50000000");
}

// ---- to_string ------------------------------------------------------------

TEST(Fixed128LiteToString, Various) {
    EXPECT_EQ(Fixed128(150000000LL, 8).to_string(), "1.50000000");
    EXPECT_EQ(Fixed128(-150000000LL, 8).to_string(), "-1.50000000");
    EXPECT_EQ(Fixed128(0LL, 8).to_string(), "0.00000000");
    EXPECT_EQ(Fixed128(0LL, 0).to_string(), "0");
    EXPECT_EQ(Fixed128(123LL, 0).to_string(), "123");
    EXPECT_EQ(Fixed128(1LL, 5).to_string(), "0.00001");
    EXPECT_EQ(Fixed128(12345LL, 2).to_string(), "123.45");
}

TEST(Fixed128LiteToString, Int64Extremes) {
    EXPECT_EQ(
        Fixed128(std::numeric_limits<int64_t>::min(), 0).to_string(),
        "-9223372036854775808");
    EXPECT_EQ(
        Fixed128(std::numeric_limits<int64_t>::max(), 0).to_string(),
        "9223372036854775807");
}

TEST(Fixed128LiteToString, NegativeExponentThrows) {
    EXPECT_THROW(Fixed128(1LL, -1).to_string(), std::domain_error);
}

// ---- from_string ----------------------------------------------------------

TEST(Fixed128LiteFromString, BasicForms) {
    EXPECT_EQ(Fixed128::from_string("1.5").to_string(),    "1.5");
    EXPECT_EQ(Fixed128::from_string("-1.5").to_string(),   "-1.5");
    EXPECT_EQ(Fixed128::from_string("+1.5").to_string(),   "1.5");
    EXPECT_EQ(Fixed128::from_string("123").to_string(),    "123");
    EXPECT_EQ(Fixed128::from_string(".5").to_string(),     "0.5");
    EXPECT_EQ(Fixed128::from_string("-.5").to_string(),    "-0.5");
    EXPECT_EQ(Fixed128::from_string("5.").to_string(),     "5");
    EXPECT_EQ(Fixed128::from_string("0").to_string(),      "0");
    EXPECT_EQ(Fixed128::from_string("-0").to_string(),     "0");
    EXPECT_EQ(Fixed128::from_string("00.10").to_string(),  "0.10");
}

TEST(Fixed128LiteFromString, ExponentIsFracLen) {
    EXPECT_EQ(Fixed128::from_string("1.500").raw_exponent(), 3);
    EXPECT_EQ(Fixed128::from_string("123").raw_exponent(),   0);
}

TEST(Fixed128LiteFromString, RoundTrip) {
    const char* cases[] = {
        "0", "1", "-1", "123", "-123", "1.5", "-1.5",
        "0.001", "123456.789", "-123456.789", "0.00000001",
    };
    for (const char* s : cases) {
        EXPECT_EQ(Fixed128::from_string(s).to_string(), s) << s;
    }
}

TEST(Fixed128LiteFromString, Errors) {
    EXPECT_THROW(Fixed128::from_string(""),       std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("-"),      std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("+"),      std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("."),      std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("abc"),    std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("1.2.3"),  std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string("1e5"),    std::invalid_argument);
    EXPECT_THROW(Fixed128::from_string(" 1.5"),   std::invalid_argument);
}

TEST(Fixed128LiteFromString, MantissaOverflow) {
    EXPECT_THROW(
        Fixed128::from_string(
            "9999999999999999999999999999999999999999"),
        std::overflow_error);
}

TEST(Fixed128LiteFromString, BeyondInt64) {
    const std::string big = "123456789012345678901234567890";
    EXPECT_EQ(Fixed128::from_string(big).to_string(), big);
}

// ---- normalize ------------------------------------------------------------

TEST(Fixed128LiteNormalize, StripsTrailingZeros) {
    auto n = Fixed128::from_string("1.500").normalize();
    EXPECT_EQ(n.raw_exponent(), 1);
    EXPECT_EQ(n.to_string(), "1.5");
}

TEST(Fixed128LiteNormalize, ZeroCanonicalised) {
    auto n = Fixed128(0LL, 8).normalize();
    EXPECT_EQ(n.raw_exponent(), 0);
    EXPECT_EQ(n.to_string(), "0");
}

TEST(Fixed128LiteNormalize, NoOp) {
    EXPECT_EQ(Fixed128::from_string("1.23").normalize().to_string(), "1.23");
}

// ---- streaming ------------------------------------------------------------

TEST(Fixed128LiteStream, OperatorOut) {
    std::ostringstream os;
    os << Fixed128::from_string("1.5");
    EXPECT_EQ(os.str(), "1.5");
}

// ---- template parameter ---------------------------------------------------

TEST(Fixed128LiteMaxPrecision, Exposed) {
    static_assert(fixed128::Fixed128<127>::max_precision == 127);
    static_assert(fixed128::Fixed128<0>::max_precision   == 0);
    SUCCEED();
}
