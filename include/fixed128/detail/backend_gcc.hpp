#pragma once
// =============================================================================
// fixed128 backend for compilers with native __int128 (GCC, Clang).
//
// Provides:
//   - int128_t  = __int128
//   - uint128_t = unsigned __int128
//   - pow10, checked_add/sub/mul/neg (with __builtin_*_overflow)
//   - unsigned_magnitude(v)  — |v| without UB for INT128_MIN
//   - int128_min()           — INT128_MIN without UB
//   - compare_cross(...)     — cross-exponent comparison without Boost
//
// Requires no external dependencies.
// =============================================================================

#if !defined(__SIZEOF_INT128__)
#error "backend_gcc.hpp requires __int128 support (GCC/Clang)"
#endif

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace fixed128 {
namespace detail {

using int128_t  = __int128;
using uint128_t = unsigned __int128;

constexpr int kMaxPower10 = 38;

inline int128_t pow10(int power) {
    if (power < 0 || power > kMaxPower10) {
        throw std::overflow_error("fixed128::pow10: exponent out of range");
    }
    int128_t result = 1;
    for (int i = 0; i < power; ++i) result *= 10;
    return result;
}

inline int128_t checked_add(int128_t a, int128_t b) {
    int128_t r;
    if (__builtin_add_overflow(a, b, &r))
        throw std::overflow_error("fixed128: addition overflow");
    return r;
}

inline int128_t checked_sub(int128_t a, int128_t b) {
    int128_t r;
    if (__builtin_sub_overflow(a, b, &r))
        throw std::overflow_error("fixed128: subtraction overflow");
    return r;
}

inline int128_t checked_mul(int128_t a, int128_t b) {
    int128_t r;
    if (__builtin_mul_overflow(a, b, &r))
        throw std::overflow_error("fixed128: multiplication overflow");
    return r;
}

inline int128_t checked_neg(int128_t a) {
    int128_t r;
    if (__builtin_sub_overflow(int128_t(0), a, &r))
        throw std::overflow_error("fixed128: negation overflow");
    return r;
}

// |v| computed via unsigned arithmetic (safe for INT128_MIN).
inline uint128_t unsigned_magnitude(int128_t v) {
    if (v < 0) return uint128_t(0) - static_cast<uint128_t>(v);
    return static_cast<uint128_t>(v);
}

inline int128_t int128_min() {
    int128_t half = int128_t(1) << 126;
    return -half - half;
}

// Cross-exponent comparison: sign(m1 * 10^-e1 - m2 * 10^-e2).
//
// No arbitrary-precision fallback; uses __builtin_mul_overflow to detect
// scaling overflow. When the scaling would overflow we know the sign from
// |scaled| > INT128_MAX >= |other|.
inline int compare_cross(int128_t m1, int8_t e1,
                         int128_t m2, int8_t e2) {
    if (e1 == e2) {
        if (m1 < m2) return -1;
        if (m1 > m2) return +1;
        return 0;
    }

    // Normalise to d > 0: compare X vs Y * 10^d.
    int128_t X = m1;
    int128_t Y = m2;
    int  d  = static_cast<int>(e1) - static_cast<int>(e2);
    bool invert = false;
    if (e1 < e2) {
        std::swap(X, Y);
        d = -d;
        invert = true;
    }

    // Zero fast paths.
    if (Y == 0) {
        int r = (X > 0) ? +1 : (X < 0) ? -1 : 0;
        return invert ? -r : r;
    }
    if (X == 0) {
        int r = (Y > 0) ? -1 : +1;
        return invert ? -r : r;
    }

    // Compute Y * 10^d with overflow detection.
    if (d > kMaxPower10) {
        int r = (Y > 0) ? -1 : +1;
        return invert ? -r : r;
    }
    int128_t Y_scaled;
    if (__builtin_mul_overflow(Y, pow10(d), &Y_scaled)) {
        int r = (Y > 0) ? -1 : +1;
        return invert ? -r : r;
    }

    int r = (X < Y_scaled) ? -1 : (X > Y_scaled) ? +1 : 0;
    return invert ? -r : r;
}

} // namespace detail
} // namespace fixed128
