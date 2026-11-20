#pragma once
// =============================================================================
// fixed128 backend for MSVC (or any compiler where the Boost backend is
// explicitly forced via FIXED128_FORCE_BOOST_BACKEND).
//
// Provides:
//   - int128_t  = boost::multiprecision::checked_int128_t
//   - uint128_t = boost::multiprecision::uint128_t
//   - pow10, checked_add/sub/mul/neg (checked type throws std::overflow_error)
//   - unsigned_magnitude(v)  — |v| via unsigned arithmetic
//   - int128_min()           — INT128_MIN
//   - compare_cross(...)     — cross-exponent comparison via cpp_int
//
// Requires Boost headers (header-only; no linking).
// =============================================================================

#include <cstdint>
#include <stdexcept>

#include <boost/multiprecision/cpp_int.hpp>

namespace fixed128 {
namespace detail {

using int128_t  = boost::multiprecision::checked_int128_t;
using uint128_t = boost::multiprecision::uint128_t;

constexpr int kMaxPower10 = 38;

inline int128_t pow10(int power) {
    if (power < 0 || power > kMaxPower10) {
        throw std::overflow_error("fixed128::pow10: exponent out of range");
    }
    int128_t result = 1;
    for (int i = 0; i < power; ++i) result *= 10;
    return result;
}

// checked_int128_t already throws std::overflow_error on arithmetic overflow,
// so these helpers are thin wrappers.
inline int128_t checked_add(int128_t a, int128_t b) { return a + b; }
inline int128_t checked_sub(int128_t a, int128_t b) { return a - b; }
inline int128_t checked_mul(int128_t a, int128_t b) { return a * b; }
inline int128_t checked_neg(int128_t a)             { return -a;  }

inline uint128_t unsigned_magnitude(int128_t v) {
    if (v < 0) return uint128_t(0) - static_cast<uint128_t>(v);
    return static_cast<uint128_t>(v);
}

inline int128_t int128_min() {
    int128_t half = int128_t(1) << 126;
    return -half - half;
}

// Cross-exponent comparison using arbitrary-precision cpp_int fallback
// (never overflows, so comparison is always total).
inline int compare_cross(int128_t m1, int8_t e1,
                         int128_t m2, int8_t e2) {
    if (e1 == e2) {
        if (m1 < m2) return -1;
        if (m1 > m2) return +1;
        return 0;
    }

    using boost::multiprecision::cpp_int;
    cpp_int left  = m1;
    cpp_int right = m2;
    if (e1 > e2) {
        for (int i = 0; i < static_cast<int>(e1) - static_cast<int>(e2); ++i)
            right *= 10;
    } else {
        for (int i = 0; i < static_cast<int>(e2) - static_cast<int>(e1); ++i)
            left *= 10;
    }

    if (left < right) return -1;
    if (left > right) return +1;
    return 0;
}

} // namespace detail
} // namespace fixed128
