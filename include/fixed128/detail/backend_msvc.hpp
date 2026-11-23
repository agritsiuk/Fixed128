#pragma once
// =============================================================================
// fixed128 backend for MSVC (or forced-Boost builds).
//
// Provides: int128_t, uint128_t, pow10, try_*, checked_*, unsigned_magnitude,
//           int128_min, compare_cross.
//
// Uses Boost.Multiprecision checked types (throw std::overflow_error on
// arithmetic overflow). Header-only; no linking required.
// =============================================================================

#include <cstdint>
#include <stdexcept>

#include <boost/multiprecision/cpp_int.hpp>

namespace fixed128 {
namespace detail {

using int128_t = boost::multiprecision::checked_int128_t;
using uint128_t = boost::multiprecision::checked_uint128_t;

constexpr int kMaxPower10 = 38;

inline int128_t pow10(int power) {
    if (power < 0 || power > kMaxPower10) {
        throw std::overflow_error("fixed128::pow10: exponent out of range");
    }
    int128_t result = 1;
    for (int i = 0; i < power; ++i) result *= 10;
    return result;
}

inline bool try_add(int128_t a, int128_t b, int128_t* r) {
    try {
        *r = a + b;
        return true;
    } catch (const std::overflow_error&) {
        return false;
    }
}
inline bool try_sub(int128_t a, int128_t b, int128_t* r) {
    try {
        *r = a - b;
        return true;
    } catch (const std::overflow_error&) {
        return false;
    }
}
inline bool try_mul(int128_t a, int128_t b, int128_t* r) {
    try {
        *r = a * b;
        return true;
    } catch (const std::overflow_error&) {
        return false;
    }
}
inline bool try_neg(int128_t a, int128_t* r) {
    try {
        *r = -a;
        return true;
    } catch (const std::overflow_error&) {
        return false;
    }
}

inline int128_t checked_add(int128_t a, int128_t b) {
    return a + b;
}
inline int128_t checked_sub(int128_t a, int128_t b) {
    return a - b;
}
inline int128_t checked_mul(int128_t a, int128_t b) {
    return a * b;
}
inline int128_t checked_neg(int128_t a) {
    return -a;
}

// |v| via unsigned arithmetic; the (-v-1)+1 trick keeps INT128_MIN safe.
inline uint128_t unsigned_magnitude(int128_t v) {
    if (v >= 0)
        return static_cast<uint128_t>(v);
    // For v = INT128_MIN:  -(v+1) = INT128_MAX  (no overflow)
    int128_t abs_minus_one = -(v + 1);
    return static_cast<uint128_t>(abs_minus_one) + 1;
}

inline int128_t int128_min() {
    int128_t half = int128_t(1) << 126;
    return -half - half;
}

// Cross-exponent comparison via cpp_int (arbitrary precision).
inline int compare_cross(int128_t m1, int8_t e1, int128_t m2, int8_t e2) {
    if (e1 == e2) {
        if (m1 < m2)
            return -1;
        if (m1 > m2)
            return +1;
        return 0;
    }

    using boost::multiprecision::cpp_int;
    cpp_int left = m1;
    cpp_int right = m2;
    int d = int(e1) - int(e2);
    if (d > 0) {
        for (int i = 0; i < d; ++i) right *= 10;
    } else {
        for (int i = 0; i < -d; ++i) left *= 10;
    }

    if (left < right)
        return -1;
    if (left > right)
        return +1;
    return 0;
}

}  // namespace detail
}  // namespace fixed128
