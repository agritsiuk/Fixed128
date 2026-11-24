#pragma once
// =============================================================================
// Fixed128 — LITE mode (MSVC fallback).
//
// Public entry point is fixed128.hpp; this file is included automatically
// for MSVC (or when FIXED128_FORCE_LITE_BACKEND is defined).
//
// Provided: from_string, to_string, normalize, from_parts, raw_* accessors.
// NOT provided: arithmetic operators, comparisons, rescale/round, hash.
// =============================================================================

#include "detail/backend_lite.hpp"

#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>

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

using detail_lite::int128_t;

template <int8_t MaxPrecision>
class Fixed128 {
    static_assert(MaxPrecision >= 0, "MaxPrecision must be non-negative");

public:
    static constexpr int8_t max_precision = MaxPrecision;

    Fixed128(int64_t source_mantissa, int8_t source_exponent) noexcept
        : mantissa_(detail_lite::from_int64(source_mantissa))
        , exponent_(source_exponent) {}

    static Fixed128 from_parts(int128_t raw_mantissa,
                               int8_t raw_exponent) noexcept {
        return from_raw(raw_mantissa, raw_exponent);
    }

    FIXED128_NODISCARD int128_t raw_mantissa() const noexcept { return mantissa_; }
    FIXED128_NODISCARD int8_t   raw_exponent() const noexcept { return exponent_; }

    FIXED128_NODISCARD std::string to_string() const {
        if (exponent_ < 0) {
            throw std::domain_error(
                "Fixed128 lite: to_string with negative exponent");
        }
        bool neg = detail_lite::is_negative(mantissa_);
        std::string digits =
            detail_lite::magnitude_to_decimal_string(mantissa_);

        if (exponent_ > 0) {
            if (int(digits.size()) <= int(exponent_)) {
                digits = std::string(
                    size_t(exponent_) - digits.size() + 1, '0') + digits;
            }
            digits.insert(digits.size() - size_t(exponent_), 1, '.');
        }
        return neg ? "-" + digits : digits;
    }

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

        int128_t m = detail_lite::parse_magnitude(
            digits.data(), digits.size());
        if (negative && !detail_lite::is_zero(m)) m.negative = true;
        return from_parts(m, static_cast<int8_t>(frac_len));
    }

    FIXED128_NODISCARD Fixed128 normalize() const noexcept {
        if (detail_lite::is_zero(mantissa_)) {
            return from_raw(detail_lite::from_int64(0), 0);
        }
        int128_t tmp = mantissa_;
        int8_t   exp = exponent_;
        while (exp > 0 && detail_lite::mod10(tmp) == 0) {
            detail_lite::div10(tmp);
            --exp;
        }
        return from_raw(tmp, exp);
    }

private:
    struct RawTag {};
    Fixed128(int128_t raw_mantissa, int8_t raw_exponent,
             RawTag) noexcept
        : mantissa_(raw_mantissa), exponent_(raw_exponent) {}

    static Fixed128 from_raw(int128_t m, int8_t e) noexcept {
        return Fixed128(m, e, RawTag{});
    }

    int128_t mantissa_;
    int8_t   exponent_;
};

template <int8_t MP>
inline std::ostream& operator<<(std::ostream& os,
                                const Fixed128<MP>& v) {
    return os << v.to_string();
}

} // namespace fixed128
