#pragma once
// =============================================================================
// Lite backend: portable 128-bit integer on 4 x uint32_t limbs.
// No <boost/*>, no __int128. Works on any C++14 compiler.
//
// Provides only what's needed for parse/serialize/normalize:
//   from_int64, is_zero, is_negative
//   mul10_add_digit, div10, mod10
//   parse_magnitude, magnitude_to_decimal_string, to_decimal_string
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace fixed128 {
namespace detail_lite {

struct int128_t {
    uint32_t limbs[4];   // limbs[0] is least significant
    bool     negative;
};

inline bool is_zero(const int128_t& v) noexcept {
    return (v.limbs[0] | v.limbs[1] | v.limbs[2] | v.limbs[3]) == 0;
}

inline bool is_negative(const int128_t& v) noexcept {
    return v.negative && !is_zero(v);
}

inline int128_t from_int64(int64_t v) noexcept {
    int128_t r;
    bool neg = v < 0;
    uint64_t mag = neg ? (uint64_t(0) - static_cast<uint64_t>(v))
                       : static_cast<uint64_t>(v);
    r.limbs[0] = static_cast<uint32_t>(mag & 0xFFFFFFFFu);
    r.limbs[1] = static_cast<uint32_t>(mag >> 32);
    r.limbs[2] = 0;
    r.limbs[3] = 0;
    r.negative = neg && (mag != 0);
    return r;
}

inline void mul10_add_digit(int128_t& v, uint32_t digit) {
    uint64_t carry = digit;
    for (int i = 0; i < 4; ++i) {
        uint64_t cur = static_cast<uint64_t>(v.limbs[i]) * 10u + carry;
        v.limbs[i] = static_cast<uint32_t>(cur & 0xFFFFFFFFu);
        carry = cur >> 32;
    }
    if (carry != 0) {
        throw std::overflow_error("Fixed128 lite: integer overflow");
    }
}

inline uint32_t div10(int128_t& v) noexcept {
    uint64_t rem = 0;
    for (int i = 3; i >= 0; --i) {
        uint64_t cur = (rem << 32) | static_cast<uint64_t>(v.limbs[i]);
        v.limbs[i] = static_cast<uint32_t>(cur / 10u);
        rem = cur % 10u;
    }
    return static_cast<uint32_t>(rem);
}

inline uint32_t mod10(const int128_t& v) noexcept {
    uint64_t rem = 0;
    for (int i = 3; i >= 0; --i) {
        uint64_t cur = (rem << 32) | static_cast<uint64_t>(v.limbs[i]);
        rem = cur % 10u;
    }
    return static_cast<uint32_t>(rem);
}

inline std::string magnitude_to_decimal_string(const int128_t& v) {
    if (is_zero(v)) return "0";
    int128_t tmp = v;
    tmp.negative = false;
    std::string out;
    while (!is_zero(tmp)) {
        out.push_back(static_cast<char>('0' + div10(tmp)));
    }
    std::reverse(out.begin(), out.end());
    return out;
}

inline int128_t parse_magnitude(const char* digits, std::size_t len) {
    int128_t r;
    r.limbs[0] = r.limbs[1] = r.limbs[2] = r.limbs[3] = 0;
    r.negative = false;
    if (len == 0) {
        throw std::invalid_argument("Fixed128 lite: no digits");
    }
    for (std::size_t i = 0; i < len; ++i) {
        char c = digits[i];
        if (c < '0' || c > '9') {
            throw std::invalid_argument("Fixed128 lite: invalid digit");
        }
        mul10_add_digit(r, static_cast<uint32_t>(c - '0'));
    }
    return r;
}

} // namespace detail_lite
} // namespace fixed128
