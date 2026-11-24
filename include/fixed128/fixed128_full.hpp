#pragma once
// =============================================================================
// Fixed128 — FULL mode (GCC / Clang with native __int128).
//
// Public entry point is fixed128.hpp; this file is included automatically
// when __SIZEOF_INT128__ is defined and FIXED128_FORCE_LITE_BACKEND is not.
//
// Includes everything from lite plus: full arithmetic, comparisons,
// rescale/round (10 rounding modes), divide_at, std::hash.
// =============================================================================

#if !defined(__SIZEOF_INT128__)
    #error "fixed128_full.hpp requires __int128 support (GCC/Clang)"
#endif

#include "detail/backend_int128.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

#if defined(__cplusplus) && __cplusplus >= 201703L
    #define FIXED128_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
    #define FIXED128_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
    #define FIXED128_NODISCARD _Check_return_
#else
    #define FIXED128_NODISCARD
#endif

namespace fixed128 {

using detail::int128_t;
using detail::uint128_t;

enum class RoundingMode {
    Truncate,      // toward zero
    Floor,         // toward -inf
    Ceiling,       // toward +inf
    AwayFromZero,  // away from zero (opposite of Truncate)
    HalfUp,        // ties away from zero
    HalfDown,      // ties toward zero
    HalfEven,      // ties to even (banker's)
    HalfOdd,       // ties to odd
    HalfFloor,     // ties toward -inf
    HalfCeiling,   // ties toward +inf
};

template <int8_t MaxPrecision>
class Fixed128 {
    static_assert(MaxPrecision >= 0,
                  "MaxPrecision must be non-negative");

public:
    static constexpr int8_t max_precision = MaxPrecision;

    // ---- constructors -------------------------------------------------------

    Fixed128(int64_t source_mantissa, int8_t source_exponent) noexcept
        : mantissa_(static_cast<int128_t>(source_mantissa))
        , exponent_(source_exponent) {}

    // 128-bit mantissa constructor. SFINAE-guarded so that `int`, `long long`,
    // etc. still prefer the int64_t overload (no ambiguity).
    template <typename T,
              typename std::enable_if<
                  std::is_same<T, int128_t>::value, int>::type = 0>
    Fixed128(T source_mantissa, int8_t source_exponent) noexcept
        : mantissa_(source_mantissa)
        , exponent_(source_exponent) {}

    static Fixed128 from_parts(int128_t raw_mantissa,
                               int8_t raw_exponent) noexcept {
        return from_raw(raw_mantissa, raw_exponent);
    }

    FIXED128_NODISCARD int128_t raw_mantissa() const noexcept { return mantissa_; }
    FIXED128_NODISCARD int8_t   raw_exponent() const noexcept { return exponent_; }

    // ---- unary --------------------------------------------------------------

    FIXED128_NODISCARD Fixed128 operator-() const {
        return from_raw(detail::checked_neg(mantissa_), exponent_);
    }
    FIXED128_NODISCARD Fixed128 operator+() const { return *this; }

    // ---- ++ / -- ------------------------------------------------------------

    Fixed128& operator++() { *this = *this + Fixed128(int64_t(1), 0); return *this; }
    Fixed128& operator--() { *this = *this - Fixed128(int64_t(1), 0); return *this; }
    Fixed128 operator++(int) { Fixed128 p = *this; ++(*this); return p; }
    Fixed128 operator--(int) { Fixed128 p = *this; --(*this); return p; }

    // ---- add / sub ----------------------------------------------------------

    FIXED128_NODISCARD Fixed128 operator+(const Fixed128& right) const {
        int8_t common = std::max(exponent_, right.exponent_);
        Fixed128 l = rescale(common);
        Fixed128 r = right.rescale(common);
        return from_raw(detail::checked_add(l.mantissa_, r.mantissa_), common);
    }
    FIXED128_NODISCARD Fixed128 operator-(const Fixed128& right) const {
        int8_t common = std::max(exponent_, right.exponent_);
        Fixed128 l = rescale(common);
        Fixed128 r = right.rescale(common);
        return from_raw(detail::checked_sub(l.mantissa_, r.mantissa_), common);
    }

    // ---- mul ----------------------------------------------------------------

    FIXED128_NODISCARD Fixed128 operator*(const Fixed128& right) const {
        // Zero on either side: canonical result, no throw.
        if (mantissa_ == 0 || right.mantissa_ == 0) {
            int natural = int(exponent_) + int(right.exponent_);
            int capped  = std::min(natural, int(MaxPrecision));
            if (capped < -128) return from_raw(0, 0);
            return from_raw(0, static_cast<int8_t>(capped));
        }

        int natural_exponent = int(exponent_) + int(right.exponent_);
        int capped_exponent  = std::min(natural_exponent, int(MaxPrecision));
        if (capped_exponent < -128) {
            throw std::overflow_error(
                "Fixed128::operator*: result exponent below int8_t range");
        }

        int128_t product = detail::checked_mul(mantissa_, right.mantissa_);
        if (capped_exponent < natural_exponent) {
            int down_shift = natural_exponent - capped_exponent;
            if (down_shift > kMaxPower10) {
                product = 0;
            } else {
                product /= detail::pow10(down_shift);
            }
        }
        return from_raw(product, static_cast<int8_t>(capped_exponent));
    }

    // ---- div ----------------------------------------------------------------

    FIXED128_NODISCARD Fixed128 operator/(const Fixed128& divisor) const {
        if (divisor.mantissa_ == 0) {
            throw std::domain_error("Fixed128::operator/: division by zero");
        }
        int scaling_shift = int(MaxPrecision)
                          - int(exponent_)
                          + int(divisor.exponent_);
        return divide_scaled(mantissa_, divisor.mantissa_, scaling_shift,
                             MaxPrecision);
    }
    FIXED128_NODISCARD Fixed128 divide_at(const Fixed128& divisor,
                                          int8_t target_exponent) const {
        if (divisor.mantissa_ == 0) {
            throw std::domain_error(
                "Fixed128::divide_at: division by zero");
        }
        int scaling_shift = int(target_exponent)
                          - int(exponent_)
                          + int(divisor.exponent_);
        return divide_scaled(mantissa_, divisor.mantissa_, scaling_shift,
                             target_exponent);
    }

    // ---- comparisons --------------------------------------------------------

    friend bool operator==(const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa_, a.exponent_,
                                     b.mantissa_, b.exponent_) == 0;
    }
    friend bool operator!=(const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa_, a.exponent_,
                                     b.mantissa_, b.exponent_) != 0;
    }
    friend bool operator< (const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa_, a.exponent_,
                                     b.mantissa_, b.exponent_) <  0;
    }
    friend bool operator<=(const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa_, a.exponent_,
                                     b.mantissa_, b.exponent_) <= 0;
    }
    friend bool operator> (const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa_, a.exponent_,
                                     b.mantissa_, b.exponent_) >  0;
    }
    friend bool operator>=(const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa_, a.exponent_,
                                     b.mantissa_, b.exponent_) >= 0;
    }

    Fixed128& operator+=(const Fixed128& r) { *this = *this + r; return *this; }
    Fixed128& operator-=(const Fixed128& r) { *this = *this - r; return *this; }
    Fixed128& operator*=(const Fixed128& r) { *this = *this * r; return *this; }
    Fixed128& operator/=(const Fixed128& r) { *this = *this / r; return *this; }

    // ---- rescale / round ----------------------------------------------------

    FIXED128_NODISCARD Fixed128 rescale(
        int8_t target_exponent,
        RoundingMode mode = RoundingMode::Truncate) const {
        if (target_exponent == exponent_) return *this;
        if (mantissa_ == 0) return from_raw(0, target_exponent);

        if (target_exponent > exponent_) {
            int up_shift = int(target_exponent) - int(exponent_);
            if (up_shift > kMaxPower10) {
                throw std::overflow_error(
                    "Fixed128::rescale: upshift too large");
            }
            int128_t scaled = detail::checked_mul(mantissa_,
                                                   detail::pow10(up_shift));
            return from_raw(scaled, target_exponent);
        }

        int down_shift = int(exponent_) - int(target_exponent);
        if (down_shift > kMaxPower10) {
            // |m| < 10^39 <= 10^down_shift: value truncates to 0.
            // Handle exactly only for Truncate; other modes conservatively
            // throw (matches original contract).
            if (mode == RoundingMode::Truncate) {
                return from_raw(0, target_exponent);
            }
            throw std::overflow_error(
                "Fixed128::rescale: downshift too large");
        }
        int128_t result = rounded_divide(mantissa_,
                                          detail::pow10(down_shift), mode);
        return from_raw(result, target_exponent);
    }

    FIXED128_NODISCARD Fixed128 round(int8_t target_exponent,
                                       RoundingMode mode) const {
        return rescale(target_exponent, mode);
    }
    FIXED128_NODISCARD Fixed128 round_half_even(int8_t t) const {
        return rescale(t, RoundingMode::HalfEven);
    }
    FIXED128_NODISCARD Fixed128 round_half_up(int8_t t) const {
        return rescale(t, RoundingMode::HalfUp);
    }
    FIXED128_NODISCARD Fixed128 round_half_odd(int8_t t) const {
        return rescale(t, RoundingMode::HalfOdd);
    }

    // ---- normalize ----------------------------------------------------------

    FIXED128_NODISCARD Fixed128 normalize() const noexcept {
        if (mantissa_ == 0) return from_raw(0, 0);
        int128_t reduced_mantissa = mantissa_;
        int8_t   reduced_exponent = exponent_;
        while (reduced_exponent > 0 && reduced_mantissa % 10 == 0) {
            reduced_mantissa /= 10;
            reduced_exponent--;
        }
        return from_raw(reduced_mantissa, reduced_exponent);
    }

    // ---- to_string ----------------------------------------------------------

    FIXED128_NODISCARD std::string to_string() const {
        if (exponent_ < 0) {
            throw std::domain_error(
                "Fixed128::to_string: negative exponent not supported");
        }
        bool is_negative = mantissa_ < 0;
        uint128_t magnitude = detail::unsigned_magnitude(mantissa_);

        std::string digits;
        if (magnitude == 0) {
            digits = "0";
        } else {
            while (magnitude > 0) {
                digits.push_back(static_cast<char>(
                    '0' + static_cast<int>(magnitude % 10)));
                magnitude /= 10;
            }
            std::reverse(digits.begin(), digits.end());
        }

        if (exponent_ > 0) {
            if (int(digits.size()) <= int(exponent_)) {
                digits = std::string(size_t(exponent_) - digits.size() + 1, '0')
                         + digits;
            }
            digits.insert(digits.size() - size_t(exponent_), 1, '.');
        }
        return is_negative ? "-" + digits : digits;
    }

    // ---- from_string --------------------------------------------------------

    static Fixed128 from_string(const std::string& s) {
        if (s.empty()) {
            throw std::invalid_argument(
                "Fixed128::from_string: empty string");
        }

        size_t i = 0;
        bool negative = false;
        if (s[i] == '+' || s[i] == '-') {
            negative = (s[i] == '-');
            ++i;
            if (i == s.size()) {
                throw std::invalid_argument(
                    "Fixed128::from_string: sign without digits");
            }
        }

        const size_t int_start = i;
        while (i < s.size() && s[i] != '.' && s[i] != 'e' && s[i] != 'E') {
            if (s[i] < '0' || s[i] > '9') {
                throw std::invalid_argument(
                    "Fixed128::from_string: invalid character");
            }
            ++i;
        }
        const size_t int_end = i;

        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            throw std::invalid_argument(
                "Fixed128::from_string: scientific notation not supported");
        }

        size_t frac_start = 0, frac_end = 0;
        bool has_dot = false;
        if (i < s.size() && s[i] == '.') {
            has_dot = true;
            ++i;
            frac_start = i;
            while (i < s.size()) {
                if (s[i] < '0' || s[i] > '9') {
                    throw std::invalid_argument(
                        "Fixed128::from_string: invalid character");
                }
                ++i;
            }
            frac_end = i;
        }

        if (int_end == int_start &&
            (!has_dot || frac_end == frac_start)) {
            throw std::invalid_argument(
                "Fixed128::from_string: no digits");
        }

        const size_t frac_len = has_dot ? (frac_end - frac_start) : 0;
        if (frac_len > 127) {
            throw std::overflow_error(
                "Fixed128::from_string: exponent out of int8_t range");
        }

        std::string digits;
        digits.reserve((int_end - int_start) + frac_len);
        digits.append(s, int_start, int_end - int_start);
        digits.append(s, frac_start, frac_len);

        int128_t m = parse_mantissa(digits.data(), digits.size());
        if (negative) m = detail::checked_neg(m);
        return from_raw(m, static_cast<int8_t>(frac_len));
    }

private:
    static constexpr int kMaxPower10 = detail::kMaxPower10;

    int128_t mantissa_;
    int8_t   exponent_;

    struct RawTag {};
    Fixed128(int128_t raw_mantissa, int8_t raw_exponent, RawTag) noexcept
        : mantissa_(raw_mantissa), exponent_(raw_exponent) {}

    static Fixed128 from_raw(int128_t m, int8_t e) noexcept {
        return Fixed128(m, e, RawTag{});
    }

    static int128_t parse_mantissa(const char* digits, std::size_t len) {
        int128_t result = 0;
        for (std::size_t i = 0; i < len; ++i) {
            char c = digits[i];
            if (c < '0' || c > '9') {
                throw std::invalid_argument(
                    "Fixed128::parse_mantissa: invalid digit");
            }
            int128_t next;
            if (!detail::try_mul(result, int128_t(10), &next)) {
                throw std::overflow_error(
                    "Fixed128::from_string: mantissa overflow");
            }
            if (!detail::try_add(next, int128_t(c - '0'), &result)) {
                throw std::overflow_error(
                    "Fixed128::from_string: mantissa overflow");
            }
        }
        return result;
    }

    static int128_t rounded_divide(int128_t dividend,
                                   int128_t positive_divisor,
                                   RoundingMode mode) {
        if (positive_divisor <= 0) {
            throw std::invalid_argument(
                "Fixed128::rounded_divide: divisor must be positive");
        }
        int128_t quotient  = dividend / positive_divisor;
        int128_t remainder = dividend % positive_divisor;
        if (remainder == 0) return quotient;

        bool negative = dividend < 0;
        uint128_t abs_rem = detail::unsigned_magnitude(remainder);
        uint128_t div_u   = static_cast<uint128_t>(positive_divisor);
        uint128_t twice   = abs_rem * 2;

        switch (mode) {
        case RoundingMode::Truncate: return quotient;
        case RoundingMode::Floor:
            return negative ? detail::checked_sub(quotient, 1) : quotient;
        case RoundingMode::Ceiling:
            return negative ? quotient : detail::checked_add(quotient, 1);
        case RoundingMode::AwayFromZero:
            return negative ? detail::checked_sub(quotient, 1)
                            : detail::checked_add(quotient, 1);
        case RoundingMode::HalfUp:
            if (twice >= div_u)
                return negative ? detail::checked_sub(quotient, 1)
                                : detail::checked_add(quotient, 1);
            return quotient;
        case RoundingMode::HalfDown:
            if (twice > div_u)
                return negative ? detail::checked_sub(quotient, 1)
                                : detail::checked_add(quotient, 1);
            return quotient;
        case RoundingMode::HalfEven:
            if (twice > div_u)
                return negative ? detail::checked_sub(quotient, 1)
                                : detail::checked_add(quotient, 1);
            if (twice < div_u) return quotient;
            if (quotient % 2 == 0) return quotient;
            return negative ? detail::checked_sub(quotient, 1)
                            : detail::checked_add(quotient, 1);
        case RoundingMode::HalfOdd:
            if (twice > div_u)
                return negative ? detail::checked_sub(quotient, 1)
                                : detail::checked_add(quotient, 1);
            if (twice < div_u) return quotient;
            if (quotient % 2 != 0) return quotient;  // already odd
            return negative ? detail::checked_sub(quotient, 1)
                            : detail::checked_add(quotient, 1);
        case RoundingMode::HalfFloor:
            if (twice > div_u)
                return negative ? detail::checked_sub(quotient, 1)
                                : detail::checked_add(quotient, 1);
            if (twice < div_u) return quotient;
            return negative ? detail::checked_sub(quotient, 1) : quotient;
        case RoundingMode::HalfCeiling:
            if (twice > div_u)
                return negative ? detail::checked_sub(quotient, 1)
                                : detail::checked_add(quotient, 1);
            if (twice < div_u) return quotient;
            return negative ? quotient : detail::checked_add(quotient, 1);
        }
        throw std::logic_error(
            "Fixed128::rounded_divide: unknown rounding mode");
    }

    static Fixed128 divide_scaled(int128_t numerator, int128_t denominator,
                                  int scaling_shift,
                                  int8_t result_exponent) {
        // Zero numerator -> zero result, no range checks needed.
        if (numerator == 0) return from_raw(0, result_exponent);

        if (denominator == -1 && numerator == detail::int128_min()) {
            throw std::overflow_error(
                "Fixed128::divide_scaled: INT128_MIN / -1");
        }

        if (scaling_shift < 0) {
            int down_shift = -scaling_shift;
            if (down_shift > kMaxPower10) {
                return from_raw(0, result_exponent);
            }
            int128_t scaled_denominator;
            if (!detail::try_mul(denominator, detail::pow10(down_shift),
                                 &scaled_denominator)) {
                return from_raw(0, result_exponent);
            }
            return from_raw(numerator / scaled_denominator, result_exponent);
        }

        if (scaling_shift == 0) {
            return from_raw(numerator / denominator, result_exponent);
        }

        if (scaling_shift > kMaxPower10) {
            throw std::overflow_error(
                "Fixed128::divide_scaled: shift too large");
        }

        int128_t integer_quotient  = numerator / denominator;
        int128_t integer_remainder = numerator % denominator;

        int128_t scaled_quotient =
            detail::checked_mul(integer_quotient,
                                 detail::pow10(scaling_shift));
        int128_t fractional_part =
            compute_fraction(integer_remainder, scaling_shift, denominator);

        int128_t result_mantissa =
            detail::checked_add(scaled_quotient, fractional_part);
        return from_raw(result_mantissa, result_exponent);
    }

    static int128_t compute_fraction(int128_t remainder, int shift,
                                     int128_t divisor) {
        if (shift > 0) {
            int128_t scaled;
            if (detail::try_mul(remainder, detail::pow10(shift), &scaled)) {
                return scaled / divisor;
            }
        }
        int128_t running_quotient  = 0;
        int128_t running_remainder = remainder;
        for (int step = 0; step < shift; ++step) {
            int128_t r10 = detail::checked_mul(running_remainder, 10);
            int128_t q10 = detail::checked_mul(running_quotient, 10);
            running_quotient  = detail::checked_add(q10, r10 / divisor);
            running_remainder = r10 % divisor;
        }
        return running_quotient;
    }
};

// -----------------------------------------------------------------------------
// int64_t overloads
// -----------------------------------------------------------------------------
template <int8_t MP>
FIXED128_NODISCARD inline bool operator==(const Fixed128<MP>& a, int64_t b) {
    return a == Fixed128<MP>(b, 0);
}
template <int8_t MP>
FIXED128_NODISCARD inline bool operator==(int64_t a, const Fixed128<MP>& b) {
    return Fixed128<MP>(a, 0) == b;
}
template <int8_t MP>
FIXED128_NODISCARD inline bool operator!=(const Fixed128<MP>& a, int64_t b) {
    return a != Fixed128<MP>(b, 0);
}
template <int8_t MP>
FIXED128_NODISCARD inline bool operator!=(int64_t a, const Fixed128<MP>& b) {
    return Fixed128<MP>(a, 0) != b;
}
template <int8_t MP>
inline Fixed128<MP>& operator+=(Fixed128<MP>& a, int64_t b) {
    a = a + Fixed128<MP>(b, 0);
    return a;
}
template <int8_t MP>
inline Fixed128<MP>& operator-=(Fixed128<MP>& a, int64_t b) {
    a = a - Fixed128<MP>(b, 0);
    return a;
}

template <int8_t MP>
inline std::ostream& operator<<(std::ostream& stream,
                                const Fixed128<MP>& value) {
    return stream << value.to_string();
}

} // namespace fixed128

// -----------------------------------------------------------------------------
// std::hash specialization
//
// Consistency contract: hash(a) == hash(b) whenever a == b, PROVIDED both
// values have non-negative exponents. For non-negative exponents, normalize()
// produces a canonical (mantissa, exponent) pair, which we hash.
//
// For values with negative exponents, normalize() leaves them unchanged, so
// equal values with different representations (e.g. Fixed128(1, -2) vs
// Fixed128(10, -3)) will hash differently. Callers using negative exponents
// as hash keys should rescale to a non-negative exponent first.
// -----------------------------------------------------------------------------
namespace std {
template <int8_t MP>
struct hash<fixed128::Fixed128<MP>> {
    std::size_t operator()(const fixed128::Fixed128<MP>& v) const noexcept {
        auto n = v.normalize();
        fixed128::uint128_t um =
            static_cast<fixed128::uint128_t>(n.raw_mantissa());
        uint64_t hi = static_cast<uint64_t>(um >> 64);
        uint64_t lo = static_cast<uint64_t>(um);
        std::size_t h = std::hash<uint64_t>{}(hi);
        h = h * 31u + std::hash<uint64_t>{}(lo);
        h = h * 31u + std::hash<int8_t>{}(n.raw_exponent());
        return h;
    }
};
} // namespace std
