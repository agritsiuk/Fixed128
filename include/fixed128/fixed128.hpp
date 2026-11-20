#pragma once
// =============================================================================
// Fixed128: header-only fixed-point arithmetic.
//
// Backend is selected automatically:
//   - native __int128 on GCC / Clang (no Boost dependency)
//   - Boost.Multiprecision on MSVC (header-only, no linking)
//   - FIXED128_FORCE_BOOST_BACKEND forces the Boost backend even on GCC/Clang
//
// Value representation:
//     value = mantissa / 10^exponent
//
// MaxPrecision (mandatory NTTP):
//   - operator* caps natural exponent (ea + eb) at MaxPrecision;
//   - operator/ always uses MaxPrecision as target exponent.
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

namespace fixed128 {

using detail::int128_t;
using detail::uint128_t;

// Rounding modes for rescaling to a lower exponent.
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

    int128_t mantissa;
    int8_t   exponent;

    // Public constructor: int64_t mantissa + int8_t exponent.
    Fixed128(int64_t source_mantissa, int8_t source_exponent) noexcept
        : mantissa(source_mantissa)
        , exponent(source_exponent) {}

    // -------------------------------------------------------------------------
    // UNARY OPERATORS
    // -------------------------------------------------------------------------

    Fixed128 operator-() const {
        return from_raw(detail::checked_neg(mantissa), exponent);
    }

    Fixed128 operator+() const {
        return *this;
    }

    // -------------------------------------------------------------------------
    // INCREMENT / DECREMENT
    //
    // ++x adds 1.0 (value = 1 at exponent 0). Result exponent becomes
    // max(exponent, 0), matching the addition rule.
    // -------------------------------------------------------------------------
    Fixed128& operator++() {
        *this = *this + Fixed128(1, 0);
        return *this;
    }
    Fixed128& operator--() {
        *this = *this - Fixed128(1, 0);
        return *this;
    }
    Fixed128 operator++(int) {
        Fixed128 previous = *this;
        ++(*this);
        return previous;
    }
    Fixed128 operator--(int) {
        Fixed128 previous = *this;
        --(*this);
        return previous;
    }

    // -------------------------------------------------------------------------
    // ADDITION / SUBTRACTION
    //
    // Common denominator = max(ea, eb). No MaxPrecision cap.
    // -------------------------------------------------------------------------
    Fixed128 operator+(const Fixed128& right_operand) const {
        int8_t common_exponent = std::max(exponent, right_operand.exponent);
        Fixed128 left_scaled  = rescale(common_exponent);
        Fixed128 right_scaled = right_operand.rescale(common_exponent);

        int128_t sum_mantissa =
            detail::checked_add(left_scaled.mantissa, right_scaled.mantissa);
        return from_raw(sum_mantissa, common_exponent);
    }

    Fixed128 operator-(const Fixed128& right_operand) const {
        int8_t common_exponent = std::max(exponent, right_operand.exponent);
        Fixed128 left_scaled  = rescale(common_exponent);
        Fixed128 right_scaled = right_operand.rescale(common_exponent);

        int128_t diff_mantissa =
            detail::checked_sub(left_scaled.mantissa, right_scaled.mantissa);
        return from_raw(diff_mantissa, common_exponent);
    }

    // -------------------------------------------------------------------------
    // MULTIPLICATION
    //
    // Natural target = ea + eb; capped at MaxPrecision (truncation toward zero).
    // -------------------------------------------------------------------------
    Fixed128 operator*(const Fixed128& right_operand) const {
        int natural_exponent = static_cast<int>(exponent)
                             + static_cast<int>(right_operand.exponent);
        int capped_exponent  = std::min(natural_exponent,
                                        static_cast<int>(MaxPrecision));

        if (capped_exponent < -128) {
            throw std::overflow_error(
                "Fixed128::operator*: result exponent below int8_t range");
        }

        int128_t product_mantissa =
            detail::checked_mul(mantissa, right_operand.mantissa);

        if (capped_exponent < natural_exponent) {
            int down_shift = natural_exponent - capped_exponent;
            if (down_shift > kMaxPower10) {
                throw std::overflow_error(
                    "Fixed128::operator*: truncation shift too large");
            }
            product_mantissa /= detail::pow10(down_shift);
        }
        return from_raw(product_mantissa,
                        static_cast<int8_t>(capped_exponent));
    }

    // -------------------------------------------------------------------------
    // DIVISION
    //
    // Target exponent is ALWAYS MaxPrecision.
    // -------------------------------------------------------------------------
    Fixed128 operator/(const Fixed128& divisor) const {
        if (divisor.mantissa == 0) {
            throw std::domain_error(
                "Fixed128::operator/: division by zero");
        }
        int scaling_shift = static_cast<int>(MaxPrecision)
                          - static_cast<int>(exponent)
                          + static_cast<int>(divisor.exponent);
        return divide_scaled(mantissa, divisor.mantissa, scaling_shift,
                             MaxPrecision);
    }

    Fixed128 divide_at(const Fixed128& divisor, int8_t target_exponent) const {
        if (divisor.mantissa == 0) {
            throw std::domain_error(
                "Fixed128::divide_at: division by zero");
        }
        int scaling_shift = static_cast<int>(target_exponent)
                          - static_cast<int>(exponent)
                          + static_cast<int>(divisor.exponent);
        return divide_scaled(mantissa, divisor.mantissa, scaling_shift,
                             target_exponent);
    }

    // -------------------------------------------------------------------------
    // COMPARISON
    //
    // Compares real values, not (mantissa, exponent) pairs. Cross-exponent
    // comparison is delegated to the backend (arbitrary precision or
    // overflow-detecting __int128).
    // -------------------------------------------------------------------------
    friend bool operator==(const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa, a.exponent,
                                     b.mantissa, b.exponent) == 0;
    }
    friend bool operator!=(const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa, a.exponent,
                                     b.mantissa, b.exponent) != 0;
    }
    friend bool operator< (const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa, a.exponent,
                                     b.mantissa, b.exponent) <  0;
    }
    friend bool operator<=(const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa, a.exponent,
                                     b.mantissa, b.exponent) <= 0;
    }
    friend bool operator> (const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa, a.exponent,
                                     b.mantissa, b.exponent) >  0;
    }
    friend bool operator>=(const Fixed128& a, const Fixed128& b) {
        return detail::compare_cross(a.mantissa, a.exponent,
                                     b.mantissa, b.exponent) >= 0;
    }

    // Compound assignment.
    Fixed128& operator+=(const Fixed128& r) { *this = *this + r; return *this; }
    Fixed128& operator-=(const Fixed128& r) { *this = *this - r; return *this; }
    Fixed128& operator*=(const Fixed128& r) { *this = *this * r; return *this; }
    Fixed128& operator/=(const Fixed128& r) { *this = *this / r; return *this; }

    // -------------------------------------------------------------------------
    // RESCALE
    // -------------------------------------------------------------------------
    Fixed128 rescale(int8_t target_exponent,
                     RoundingMode mode = RoundingMode::Truncate) const {
        if (target_exponent == exponent) return *this;

        if (target_exponent > exponent) {
            int up_shift = static_cast<int>(target_exponent)
                         - static_cast<int>(exponent);
            if (up_shift > kMaxPower10) {
                throw std::overflow_error(
                    "Fixed128::rescale: upshift too large");
            }
            int128_t scaled_mantissa =
                detail::checked_mul(mantissa, detail::pow10(up_shift));
            return from_raw(scaled_mantissa, target_exponent);
        }

        int down_shift = static_cast<int>(exponent)
                       - static_cast<int>(target_exponent);
        if (down_shift > kMaxPower10) {
            throw std::overflow_error(
                "Fixed128::rescale: downshift too large");
        }
        int128_t result = rounded_divide(mantissa,
                                         detail::pow10(down_shift), mode);
        return from_raw(result, target_exponent);
    }

    Fixed128 round(int8_t target_exponent, RoundingMode mode) const {
        return rescale(target_exponent, mode);
    }
    Fixed128 round_half_even(int8_t target_exponent) const {
        return rescale(target_exponent, RoundingMode::HalfEven);
    }
    Fixed128 round_half_up(int8_t target_exponent) const {
        return rescale(target_exponent, RoundingMode::HalfUp);
    }

    // -------------------------------------------------------------------------
    // NORMALIZE
    // -------------------------------------------------------------------------
    Fixed128 normalize() const {
        if (mantissa == 0) return from_raw(0, 0);
        int128_t reduced_mantissa = mantissa;
        int8_t   reduced_exponent = exponent;
        while (reduced_exponent > 0 && reduced_mantissa % 10 == 0) {
            reduced_mantissa /= 10;
            reduced_exponent--;
        }
        return from_raw(reduced_mantissa, reduced_exponent);
    }

    // -------------------------------------------------------------------------
    // STRING CONVERSION
    // -------------------------------------------------------------------------
    std::string to_string() const {
        if (exponent < 0) {
            throw std::domain_error(
                "Fixed128::to_string: negative exponent not supported");
        }

        bool is_negative = mantissa < 0;
        uint128_t magnitude = detail::unsigned_magnitude(mantissa);

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

        if (exponent > 0) {
            if (static_cast<int>(digits.size()) <= static_cast<int>(exponent)) {
                digits = std::string(
                    static_cast<size_t>(exponent) - digits.size() + 1, '0')
                    + digits;
            }
            digits.insert(digits.size() - static_cast<size_t>(exponent),
                          1, '.');
        }

        return is_negative ? "-" + digits : digits;
    }

private:
    static constexpr int kMaxPower10 = detail::kMaxPower10;

    enum class RawTag { Tag };

    Fixed128(int128_t raw_mantissa, int8_t raw_exponent, RawTag) noexcept
        : mantissa(raw_mantissa), exponent(raw_exponent) {}

    static Fixed128 from_raw(int128_t raw_mantissa,
                             int8_t raw_exponent) noexcept {
        return Fixed128(raw_mantissa, raw_exponent, RawTag::Tag);
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

        bool value_negative = dividend < 0;
        uint128_t abs_remainder = detail::unsigned_magnitude(remainder);
        uint128_t divisor_u = static_cast<uint128_t>(positive_divisor);
        uint128_t twice_remainder = abs_remainder * 2;

        switch (mode) {
        case RoundingMode::Truncate:
            return quotient;
        case RoundingMode::Floor:
            return value_negative
                ? detail::checked_sub(quotient, 1) : quotient;
        case RoundingMode::Ceiling:
            return value_negative
                ? quotient : detail::checked_add(quotient, 1);
        case RoundingMode::HalfUp:
            if (twice_remainder >= divisor_u)
                return value_negative ? detail::checked_sub(quotient, 1)
                                      : detail::checked_add(quotient, 1);
            return quotient;
        case RoundingMode::HalfDown:
            if (twice_remainder > divisor_u)
                return value_negative ? detail::checked_sub(quotient, 1)
                                      : detail::checked_add(quotient, 1);
            return quotient;
        case RoundingMode::HalfEven:
            if (twice_remainder > divisor_u)
                return value_negative ? detail::checked_sub(quotient, 1)
                                      : detail::checked_add(quotient, 1);
            if (twice_remainder < divisor_u) return quotient;
            if (quotient % 2 == 0) return quotient;
            return value_negative ? detail::checked_sub(quotient, 1)
                                  : detail::checked_add(quotient, 1);
        case RoundingMode::HalfFloor:
            if (twice_remainder > divisor_u)
                return value_negative ? detail::checked_sub(quotient, 1)
                                      : detail::checked_add(quotient, 1);
            if (twice_remainder < divisor_u) return quotient;
            return value_negative ? detail::checked_sub(quotient, 1)
                                  : quotient;
        case RoundingMode::HalfCeiling:
            if (twice_remainder > divisor_u)
                return value_negative ? detail::checked_sub(quotient, 1)
                                      : detail::checked_add(quotient, 1);
            if (twice_remainder < divisor_u) return quotient;
            return value_negative ? quotient
                                  : detail::checked_add(quotient, 1);
        }
        throw std::logic_error(
            "Fixed128::rounded_divide: unknown rounding mode");
    }

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
                throw std::overflow_error(
                    "Fixed128::divide_scaled: shift too large");
            }
            int128_t scaled_denominator =
                detail::checked_mul(denominator, detail::pow10(down_shift));
            return from_raw(numerator / scaled_denominator,
                            result_exponent);
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

    // trunc(remainder * 10^shift / divisor) via stepwise long division.
    static int128_t compute_fraction(int128_t remainder, int shift,
                                     int128_t divisor) {
        int128_t running_quotient  = 0;
        int128_t running_remainder = remainder;
        for (int step = 0; step < shift; ++step) {
            int128_t remainder_times_ten =
                detail::checked_mul(running_remainder, 10);
            int128_t quotient_times_ten  =
                detail::checked_mul(running_quotient, 10);
            running_quotient = detail::checked_add(
                quotient_times_ten, remainder_times_ten / divisor);
            running_remainder = remainder_times_ten % divisor;
        }
        return running_quotient;
    }
};

template <int8_t MaxPrecision>
inline std::ostream& operator<<(std::ostream& stream,
                                const Fixed128<MaxPrecision>& value) {
    return stream << value.to_string();
}

} // namespace fixed128
