#pragma once
// =============================================================================
// fixed128: header-only fixed-point arithmetic on top of __int128.
//
// Value representation:
//     value = mantissa / 10^exponent
// where `mantissa` is a signed 128-bit integer and `exponent` is an int8_t
// giving the number of decimal places.
//
// The template parameter MaxPrecision is REQUIRED and caps the *automatic*
// target exponent derived by operator* and operator/. If the natural
// exponent exceeds MaxPrecision, the mantissa is truncated toward zero to
// bring it down.
//
// Operations that take an explicit target (rescale, divide_at) and the
// additive operators (+ and -) do NOT apply the cap: those represent
// user-intent operations rather than derived-precision ones.
// =============================================================================

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>

namespace fixed128 {

// Rounding modes for rescaling to a lower exponent.
enum class RoundingMode {
    Truncate,     // toward zero
    Floor,        // toward -inf
    Ceiling,      // toward +inf
    HalfUp,       // ties away from zero (like std::round)
    HalfDown,     // ties toward zero
    HalfEven,     // bankers' rounding (ties to even)
    HalfFloor,    // ties toward -inf
    HalfCeiling,  // ties toward +inf
};

template <int8_t MaxPrecision>
class Fixed128 {
    static_assert(MaxPrecision >= 0,
                  "MaxPrecision must be non-negative");

public:
    static constexpr int8_t max_precision = MaxPrecision;

    __int128 mantissa;
    int8_t   exponent;

    // Public constructor: int64_t mantissa + int8_t exponent.
    // User-provided inputs are kept in a range that fits inside __int128
    // for standard one-step operations.
    Fixed128(int64_t source_mantissa, int8_t source_exponent) noexcept
        : mantissa(static_cast<__int128>(source_mantissa))
        , exponent(source_exponent) {}

    // -------------------------------------------------------------------------
    // ADDITION
    //
    // Common denominator = max(ea, eb). The less precise operand is scaled up
    // to the more precise one, which preserves maximum information.
    // No MaxPrecision cap: addition is a user-intent operation.
    // -------------------------------------------------------------------------
    Fixed128 operator+(const Fixed128& right_operand) const {
        int8_t common_exponent = std::max(exponent, right_operand.exponent);
        Fixed128 left_scaled  = rescale(common_exponent);
        Fixed128 right_scaled = right_operand.rescale(common_exponent);

        __int128 sum_mantissa;
        if (__builtin_add_overflow(left_scaled.mantissa,
                                   right_scaled.mantissa, &sum_mantissa)) {
            throw std::overflow_error(
                "Fixed128::operator+: mantissa overflow");
        }
        return from_raw(sum_mantissa, common_exponent);
    }

    // -------------------------------------------------------------------------
    // SUBTRACTION
    //
    // Same common-denominator rule as addition.
    // -------------------------------------------------------------------------
    Fixed128 operator-(const Fixed128& right_operand) const {
        int8_t common_exponent = std::max(exponent, right_operand.exponent);
        Fixed128 left_scaled  = rescale(common_exponent);
        Fixed128 right_scaled = right_operand.rescale(common_exponent);

        __int128 diff_mantissa;
        if (__builtin_sub_overflow(left_scaled.mantissa,
                                   right_scaled.mantissa, &diff_mantissa)) {
            throw std::overflow_error(
                "Fixed128::operator-: mantissa overflow");
        }
        return from_raw(diff_mantissa, common_exponent);
    }

    // -------------------------------------------------------------------------
    // MULTIPLICATION
    //
    // Natural target = ea + eb (mantissa product is exact).
    // If natural > MaxPrecision, cap and truncate toward zero.
    //
    // LIMITATION (phantom overflow): the raw product is computed before the
    // cap is applied. For int64_t-mantissa inputs (|m| < 2^63) this is safe
    // because |ma*mb| < 2^126. For chained "*" on already-scaled values
    // whose mantissas approach INT128_MAX, the raw product may overflow
    // even though the capped result would fit; then std::overflow_error
    // is thrown rather than producing a wrong answer.
    // -------------------------------------------------------------------------
    Fixed128 operator*(const Fixed128& right_operand) const {
        int natural_exponent = static_cast<int>(exponent)
                             + static_cast<int>(right_operand.exponent);
        int capped_exponent  = std::min(natural_exponent,
                                        static_cast<int>(MaxPrecision));

        // Guard: natural can be as low as -256. int8_t cannot represent that.
        if (capped_exponent < -128) {
            throw std::overflow_error(
                "Fixed128::operator*: result exponent below int8_t range");
        }

        __int128 product_mantissa;
        if (__builtin_mul_overflow(mantissa, right_operand.mantissa,
                                   &product_mantissa)) {
            throw std::overflow_error(
                "Fixed128::operator*: mantissa overflow");
        }

        if (capped_exponent < natural_exponent) {
            int down_shift = natural_exponent - capped_exponent;
            if (down_shift > kMaxPower10) {
                throw std::overflow_error(
                    "Fixed128::operator*: truncation shift too large");
            }
            product_mantissa /= pow10(down_shift);
        }
        return from_raw(product_mantissa,
                        static_cast<int8_t>(capped_exponent));
    }

    // -------------------------------------------------------------------------
    // DIVISION
    //
    // Natural target = max(ea, eb). Division never adds significant digits,
    // so we adopt the precision of the better operand — this matches the
    // additive operators and preserves "accuracy = accuracy of best input".
    // If natural > MaxPrecision, cap and adjust the scaling shift accordingly.
    // -------------------------------------------------------------------------
    Fixed128 operator/(const Fixed128& divisor) const {
        if (divisor.mantissa == 0) {
            throw std::domain_error(
                "Fixed128::operator/: division by zero");
        }
        int natural_target = std::max(static_cast<int>(exponent),
                                      static_cast<int>(divisor.exponent));
        int capped_target  = std::min(natural_target,
                                      static_cast<int>(MaxPrecision));
        int scaling_shift  = capped_target
                           - static_cast<int>(exponent)
                           + static_cast<int>(divisor.exponent);
        return divide_scaled(mantissa, divisor.mantissa, scaling_shift,
                             static_cast<int8_t>(capped_target));
    }

    // -------------------------------------------------------------------------
    // EXPLICIT-TARGET DIVISION
    //
    // The target is fully user-controlled and is NOT capped by MaxPrecision.
    // -------------------------------------------------------------------------
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

    // Compound assignment operators delegate to the binary forms.
    Fixed128& operator+=(const Fixed128& right_operand) {
        *this = *this + right_operand; return *this;
    }
    Fixed128& operator-=(const Fixed128& right_operand) {
        *this = *this - right_operand; return *this;
    }
    Fixed128& operator*=(const Fixed128& right_operand) {
        *this = *this * right_operand; return *this;
    }
    Fixed128& operator/=(const Fixed128& right_operand) {
        *this = *this / right_operand; return *this;
    }

    // -------------------------------------------------------------------------
    // RESCALE
    //
    // Upscaling (target > exponent) is exact: multiply mantissa by 10^diff.
    // Downscaling applies `mode` (default: Truncate).
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
            __int128 scaled_mantissa;
            if (__builtin_mul_overflow(mantissa, pow10(up_shift),
                                       &scaled_mantissa)) {
                throw std::overflow_error(
                    "Fixed128::rescale: mantissa overflow");
            }
            return from_raw(scaled_mantissa, target_exponent);
        }

        int down_shift = static_cast<int>(exponent)
                       - static_cast<int>(target_exponent);
        if (down_shift > kMaxPower10) {
            throw std::overflow_error(
                "Fixed128::rescale: downshift too large");
        }
        __int128 result = rounded_divide(mantissa, pow10(down_shift), mode);
        return from_raw(result, target_exponent);
    }

    // Convenience wrappers.
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
    //
    // Strips trailing zeros from the mantissa, reducing the exponent down
    // to (but not below) zero. Zero is canonicalized to (0, 0).
    //
    // Negative exponents are left untouched: this operation targets the
    // financial fixed-point case where exponent >= 0. If you need to
    // canonicalize negative exponents, do so manually.
    // -------------------------------------------------------------------------
    Fixed128 normalize() const {
        if (mantissa == 0) return from_raw(0, 0);
        __int128 reduced_mantissa = mantissa;
        int8_t   reduced_exponent = exponent;
        while (reduced_exponent > 0 && reduced_mantissa % 10 == 0) {
            reduced_mantissa /= 10;
            reduced_exponent--;
        }
        return from_raw(reduced_mantissa, reduced_exponent);
    }

    // -------------------------------------------------------------------------
    // STRING CONVERSION
    //
    // Only exponent >= 0 is supported (financial use case).
    // Output includes exactly `exponent` digits after the decimal point.
    //
    // Absolute value is computed via unsigned __int128 to avoid UB for
    // mantissa == INT128_MIN.
    // -------------------------------------------------------------------------
    std::string to_string() const {
        if (exponent < 0) {
            throw std::domain_error(
                "Fixed128::to_string: negative exponent not supported");
        }

        bool is_negative = mantissa < 0;

        // |mantissa| without UB.
        // For negative mantissa:
        //     static_cast<unsigned __int128>(m) = 2^128 - |m|
        //     0 - (2^128 - |m|) = |m|  (mod 2^128)
        // For INT128_MIN: |INT128_MIN| = 2^127, still fits in unsigned __int128.
        unsigned __int128 magnitude;
        if (is_negative) {
            magnitude = 0 - static_cast<unsigned __int128>(mantissa);
        } else {
            magnitude = static_cast<unsigned __int128>(mantissa);
        }

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
    // Largest power of 10 that fits in __int128 (10^38 < INT128_MAX).
    static constexpr int kMaxPower10 = 38;

    // Internal tag: allows constructing Fixed128 from __int128 without
    // going through the public int64_t-typed constructor.
    enum class RawTag { Tag };

    Fixed128(__int128 raw_mantissa, int8_t raw_exponent, RawTag) noexcept
        : mantissa(raw_mantissa), exponent(raw_exponent) {}

    static Fixed128 from_raw(__int128 raw_mantissa,
                             int8_t raw_exponent) noexcept {
        return Fixed128(raw_mantissa, raw_exponent, RawTag::Tag);
    }

    // 10^power for power in [0, kMaxPower10].
    static __int128 pow10(int power) {
        if (power < 0 || power > kMaxPower10) {
            throw std::overflow_error(
                "Fixed128::pow10: exponent out of range");
        }
        __int128 result = 1;
        for (int i = 0; i < power; ++i) result *= 10;
        return result;
    }

    // -------------------------------------------------------------------------
    // Divide `dividend` by a positive `divisor`, applying the specified mode.
    // Caller guarantees divisor > 0.
    // -------------------------------------------------------------------------
    static __int128 rounded_divide(__int128 dividend,
                                   __int128 positive_divisor,
                                   RoundingMode mode) {
        if (positive_divisor <= 0) {
            throw std::invalid_argument(
                "Fixed128::rounded_divide: divisor must be positive");
        }

        __int128 quotient  = dividend / positive_divisor;
        __int128 remainder = dividend % positive_divisor;
        if (remainder == 0) return quotient;

        bool value_negative = dividend < 0;
        unsigned __int128 abs_remainder = value_negative
            ? static_cast<unsigned __int128>(-remainder)
            : static_cast<unsigned __int128>(remainder);
        unsigned __int128 divisor_u =
            static_cast<unsigned __int128>(positive_divisor);
        unsigned __int128 twice_remainder = abs_remainder * 2;  // < 2^128

        switch (mode) {
        case RoundingMode::Truncate:
            return quotient;
        case RoundingMode::Floor:
            return value_negative ? quotient - 1 : quotient;
        case RoundingMode::Ceiling:
            return value_negative ? quotient : quotient + 1;
        case RoundingMode::HalfUp:
            if (twice_remainder >= divisor_u)
                return value_negative ? quotient - 1 : quotient + 1;
            return quotient;
        case RoundingMode::HalfDown:
            if (twice_remainder > divisor_u)
                return value_negative ? quotient - 1 : quotient + 1;
            return quotient;
        case RoundingMode::HalfEven:
            if (twice_remainder > divisor_u)
                return value_negative ? quotient - 1 : quotient + 1;
            if (twice_remainder < divisor_u) return quotient;
            // Exact tie: round to even.
            if (quotient % 2 == 0) return quotient;
            return value_negative ? quotient - 1 : quotient + 1;
        case RoundingMode::HalfFloor:
            if (twice_remainder > divisor_u)
                return value_negative ? quotient - 1 : quotient + 1;
            if (twice_remainder < divisor_u) return quotient;
            // Exact tie: toward -inf.
            return value_negative ? quotient - 1 : quotient;
        case RoundingMode::HalfCeiling:
            if (twice_remainder > divisor_u)
                return value_negative ? quotient - 1 : quotient + 1;
            if (twice_remainder < divisor_u) return quotient;
            // Exact tie: toward +inf.
            return value_negative ? quotient : quotient + 1;
        }
        throw std::logic_error(
            "Fixed128::rounded_divide: unknown rounding mode");
    }

    // -------------------------------------------------------------------------
    // R/10^result_exponent = (numerator * 10^scaling_shift) / denominator
    //
    // Computed via divmod:
    //     q, rem = divmod(numerator, denominator)
    //     R = q * 10^shift + trunc(rem * 10^shift / denominator)
    //
    // This avoids overflow of `numerator * 10^scaling_shift` for typical
    // int64_t-mantissa inputs.
    // -------------------------------------------------------------------------
    static Fixed128 divide_scaled(__int128 numerator, __int128 denominator,
                                  int scaling_shift,
                                  int8_t result_exponent) {
        // Guard against UB: INT128_MIN / -1.
        if (denominator == -1 && numerator == int128_minimum()) {
            throw std::overflow_error(
                "Fixed128::divide_scaled: INT128_MIN / -1");
        }

        // Negative shift: divide both mantissas by 10^|shift| via the divisor.
        if (scaling_shift < 0) {
            int down_shift = -scaling_shift;
            if (down_shift > kMaxPower10) {
                throw std::overflow_error(
                    "Fixed128::divide_scaled: shift too large");
            }
            __int128 scaled_denominator;
            if (__builtin_mul_overflow(denominator, pow10(down_shift),
                                       &scaled_denominator)) {
                throw std::overflow_error(
                    "Fixed128::divide_scaled: divisor overflow");
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

        __int128 integer_quotient  = numerator / denominator;
        __int128 integer_remainder = numerator % denominator;

        __int128 scaled_quotient;
        if (__builtin_mul_overflow(integer_quotient, pow10(scaling_shift),
                                   &scaled_quotient)) {
            throw std::overflow_error(
                "Fixed128::divide_scaled: quotient overflow");
        }

        __int128 fractional_part;
        if (!compute_fraction(integer_remainder, scaling_shift,
                              denominator, &fractional_part)) {
            throw std::overflow_error(
                "Fixed128::divide_scaled: fraction overflow");
        }

        __int128 result_mantissa;
        if (__builtin_add_overflow(scaled_quotient, fractional_part,
                                   &result_mantissa)) {
            throw std::overflow_error(
                "Fixed128::divide_scaled: result overflow");
        }

        return from_raw(result_mantissa, result_exponent);
    }

    // -------------------------------------------------------------------------
    // trunc(remainder * 10^shift / divisor).
    //
    // Fast path: direct multiplication if it fits.
    // Slow path: step-by-step long division (truncation toward zero matches
    // C++ integer division even for mixed signs).
    //
    // LIMITATION: if |remainder| > INT128_MAX / 10 and shift > 0 and the
    // fast path overflows, the stepwise multiplication can also overflow;
    // then `false` is returned and the caller throws. For int64_t-mantissa
    // inputs (|divisor| <= 2^63, |remainder| < 2^63) this never happens.
    // -------------------------------------------------------------------------
    static bool compute_fraction(__int128 remainder, int shift,
                                 __int128 divisor, __int128* out_fraction) {
        __int128 scaled_remainder;
        if (!__builtin_mul_overflow(remainder, pow10(shift),
                                    &scaled_remainder)) {
            *out_fraction = scaled_remainder / divisor;
            return true;
        }

        __int128 running_quotient  = 0;
        __int128 running_remainder = remainder;
        for (int step = 0; step < shift; ++step) {
            __int128 remainder_times_ten, quotient_times_ten, new_quotient;
            if (__builtin_mul_overflow(running_remainder,
                                       static_cast<__int128>(10),
                                       &remainder_times_ten)) return false;
            if (__builtin_mul_overflow(running_quotient,
                                       static_cast<__int128>(10),
                                       &quotient_times_ten)) return false;
            if (__builtin_add_overflow(quotient_times_ten,
                                       remainder_times_ten / divisor,
                                       &new_quotient)) return false;
            running_quotient  = new_quotient;
            running_remainder = remainder_times_ten % divisor;
        }
        *out_fraction = running_quotient;
        return true;
    }

    // Minimum value of __int128 (-2^127) without UB.
    static __int128 int128_minimum() {
        const __int128 half = static_cast<__int128>(1) << 126;
        return -half - half;
    }
};

template <int8_t MaxPrecision>
inline std::ostream& operator<<(std::ostream& stream,
                                const Fixed128<MaxPrecision>& value) {
    return stream << value.to_string();
}

} // namespace fixed128
