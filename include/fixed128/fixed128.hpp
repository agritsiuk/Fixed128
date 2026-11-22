#pragma once
// =============================================================================
// Fixed128: header-only fixed-point arithmetic.
//
// Backend auto-selected:
//   - native __int128 on GCC / Clang (no Boost dependency)
//   - Boost.Multiprecision on MSVC (header-only, no linking)
//   - FIXED128_FORCE_BOOST_BACKEND forces Boost everywhere
//
// Value representation:
//     value = mantissa / 10^exponent
//
// MaxPrecision (mandatory NTTP):
//   - operator* caps natural exponent (ea + eb) at MaxPrecision;
//   - operator/ ALWAYS uses MaxPrecision as target exponent.
//
// Explicit operations (rescale, divide_at) and additive operators (+/-)
// do NOT use MaxPrecision.
// =============================================================================

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>

#if defined(FIXED128_FORCE_BOOST_BACKEND)
    #include "detail/backend_msvc.hpp"
#elif defined(__SIZEOF_INT128__)
    #include "detail/backend_gcc.hpp"
#elif defined(_MSC_VER)
    #include "detail/backend_msvc.hpp"
#else
    #error "fixed128: unsupported compiler (need __int128 or MSVC)"
#endif

// [[nodiscard]] substitute that works in C++14 as well.
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
    Truncate, Floor, Ceiling,
    HalfUp, HalfDown, HalfEven, HalfFloor, HalfCeiling,
};

template <int8_t MaxPrecision>
class Fixed128 {
    static_assert(MaxPrecision >= 0,
                  "MaxPrecision must be non-negative");

public:
    static constexpr int8_t max_precision = MaxPrecision;

    // Public constructor: int64_t mantissa + int8_t exponent.
    Fixed128(int64_t source_mantissa, int8_t source_exponent) noexcept
        : mantissa_(static_cast<int128_t>(source_mantissa))
        , exponent_(source_exponent) {}

    // Construct directly from a 128-bit mantissa (needed for deserialisation
    // and tests that exceed int64_t range). Overflow is not checked: pass a
    // value that already fits in int128_t.
    static Fixed128 from_parts(int128_t raw_mantissa,
                               int8_t raw_exponent) noexcept {
        return from_raw(raw_mantissa, raw_exponent);
    }

    // Read-only accessors. Direct mutation is intentionally impossible so
    // that backend overflow behaviour (checked on MSVC, UB-prone on GCC with
    // raw __int128) never leaks through a public field.
    FIXED128_NODISCARD int128_t raw_mantissa() const noexcept { return mantissa_; }
    FIXED128_NODISCARD int8_t   raw_exponent() const noexcept { return exponent_; }

    // -------------------------------------------------------------------------
    // UNARY OPERATORS
    // -------------------------------------------------------------------------
    FIXED128_NODISCARD Fixed128 operator-() const {
        return from_raw(detail::checked_neg(mantissa_), exponent_);
    }
    FIXED128_NODISCARD Fixed128 operator+() const { return *this; }

    // -------------------------------------------------------------------------
    // INCREMENT / DECREMENT
    //
    // ++x adds 1.0 (value +1 at exponent 0). Result exponent becomes
    // max(exponent, 0), consistent with the addition rule.
    // -------------------------------------------------------------------------
    Fixed128& operator++() { *this = *this + Fixed128(1, 0); return *this; }
    Fixed128& operator--() { *this = *this - Fixed128(1, 0); return *this; }
    Fixed128 operator++(int) { Fixed128 p = *this; ++(*this); return p; }
    Fixed128 operator--(int) { Fixed128 p = *this; --(*this); return p; }

    // -------------------------------------------------------------------------
    // ADDITION / SUBTRACTION
    //
    // Common denominator = max(ea, eb). No MaxPrecision cap.
    //
    // LIMITATION: if the exponent gap between operands is very large, the
    // upscale of the smaller operand may overflow int128_t and
    // std::overflow_error is thrown even though the mathematical result
    // would fit. Use explicit rescale() to control this.
    // -------------------------------------------------------------------------
    FIXED128_NODISCARD Fixed128 operator+(const Fixed128& right_operand) const {
        int8_t common = std::max(exponent_, right_operand.exponent_);
        Fixed128 left_scaled  = rescale(common);
        Fixed128 right_scaled = right_operand.rescale(common);
        int128_t sum = detail::checked_add(left_scaled.mantissa_,
                                            right_scaled.mantissa_);
        return from_raw(sum, common);
    }

    FIXED128_NODISCARD Fixed128 operator-(const Fixed128& right_operand) const {
        int8_t common = std::max(exponent_, right_operand.exponent_);
        Fixed128 left_scaled  = rescale(common);
        Fixed128 right_scaled = right_operand.rescale(common);
        int128_t diff = detail::checked_sub(left_scaled.mantissa_,
                                             right_scaled.mantissa_);
        return from_raw(diff, common);
    }

    // -------------------------------------------------------------------------
    // MULTIPLICATION
    //
    // Natural target = ea + eb; capped at MaxPrecision (truncation toward
    // zero). When capping needs a down_shift > 38, the mathematical result
    // is guaranteed to truncate to 0 (|product| < INT128_MAX < 10^39).
    // -------------------------------------------------------------------------
    FIXED128_NODISCARD Fixed128 operator*(const Fixed128& right_operand) const {
        int natural_exponent = int(exponent_) + int(right_operand.exponent_);
        int capped_exponent  = std::min(natural_exponent,
                                        int(MaxPrecision));
        if (capped_exponent < -128) {
            throw std::overflow_error(
                "Fixed128::operator*: result exponent below int8_t range");
        }

        int128_t product = detail::checked_mul(mantissa_,
                                                right_operand.mantissa_);

        if (capped_exponent < natural_exponent) {
            int down_shift = natural_exponent - capped_exponent;
            if (down_shift > kMaxPower10) {
                product = 0;   // |product| < 10^down_shift  ⇒  truncates to 0
            } else {
                product /= detail::pow10(down_shift);
            }
        }
        return from_raw(product, static_cast<int8_t>(capped_exponent));
    }

    // -------------------------------------------------------------------------
    // DIVISION
    //
    // Target exponent is ALWAYS MaxPrecision.
    //
    // LIMITATION: scaling_shift = MaxPrecision - ea + eb must not exceed 38
    // (pow10 guard). For typical financial inputs (ea = eb) this means
    // MaxPrecision ≤ 38. Values above ~19-20 combined with already-scaled
    // numerators risk overflow. Use divide_at(divisor, target) to bypass.
    // -------------------------------------------------------------------------
    FIXED128_NODISCARD Fixed128 operator/(const Fixed128& divisor) const {
        if (divisor.mantissa_ == 0) {
            throw std::domain_error(
                "Fixed128::operator/: division by zero");
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

    // -------------------------------------------------------------------------
    // COMPARISON
    //
    // Compares real values across differing exponents. Backend-specific
    // compare_cross is total (never throws).
    // -------------------------------------------------------------------------
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

    // Compound assignment.
    Fixed128& operator+=(const Fixed128& r) { *this = *this + r; return *this; }
    Fixed128& operator-=(const Fixed128& r) { *this = *this - r; return *this; }
    Fixed128& operator*=(const Fixed128& r) { *this = *this * r; return *this; }
    Fixed128& operator/=(const Fixed128& r) { *this = *this / r; return *this; }

    // -------------------------------------------------------------------------
    // RESCALE
    //
    // Upscaling is exact (multiply by 10^diff). Downscaling applies `mode`.
    // Zero mantissa upscales to zero without overflow checks.
    // -------------------------------------------------------------------------
    FIXED128_NODISCARD Fixed128 rescale(int8_t target_exponent,
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
    FIXED128_NODISCARD Fixed128 round_half_even(int8_t target_exponent) const {
        return rescale(target_exponent, RoundingMode::HalfEven);
    }
    FIXED128_NODISCARD Fixed128 round_half_up(int8_t target_exponent) const {
        return rescale(target_exponent, RoundingMode::HalfUp);
    }

    // -------------------------------------------------------------------------
    // NORMALIZE
    //
    // Strips trailing zeros; zero canonicalised to (0, 0). Negative
    // exponents are left as-is (financial use case).
    // -------------------------------------------------------------------------
    FIXED128_NODISCARD Fixed128 normalize() const {
        if (mantissa_ == 0) return from_raw(0, 0);
        int128_t reduced_mantissa = mantissa_;
        int8_t   reduced_exponent = exponent_;
        while (reduced_exponent > 0 && reduced_mantissa % 10 == 0) {
            reduced_mantissa /= 10;
            reduced_exponent--;
        }
        return from_raw(reduced_mantissa, reduced_exponent);
    }

    // -------------------------------------------------------------------------
    // STRING CONVERSION
    //
    // Supports only exponent >= 0. Absolute value via uint128_t (safe for
    // INT128_MIN on both backends).
    // -------------------------------------------------------------------------
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

private:
    static constexpr int kMaxPower10 = detail::kMaxPower10;

    int128_t mantissa_;
    int8_t   exponent_;

    enum class RawTag { Tag };

    Fixed128(int128_t raw_mantissa, int8_t raw_exponent, RawTag) noexcept
        : mantissa_(raw_mantissa), exponent_(raw_exponent) {}

    static Fixed128 from_raw(int128_t raw_mantissa,
                             int8_t raw_exponent) noexcept {
        return Fixed128(raw_mantissa, raw_exponent, RawTag::Tag);
    }

    // -------------------------------------------------------------------------
    // Rounded division by a positive divisor.
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // R = (num * 10^shift) / den.
    //
    // If shift < 0 and the scaling of `den` overflows int128, the result
    // mathematically truncates to 0 (|num| ≤ INT128_MAX < |den·10^k|).
    // -------------------------------------------------------------------------
    static Fixed128 divide_scaled(int128_t numerator, int128_t denominator,
                                  int scaling_shift,
                                  int8_t result_exponent) {
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

    // trunc(remainder * 10^shift / divisor).
    // Fast path: one multiplication when it fits in int128.
    // Slow path: stepwise long division (matches C++ truncation for mixed
    //            signs; can throw if |remainder| > INT128_MAX / 10).
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
// Free functions: comparison / compound assignment with int64_t.
// int64_t is treated as an integer (value = b at exponent 0).
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
