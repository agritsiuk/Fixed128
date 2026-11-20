# Fixed128

Header-only C++ fixed-point arithmetic library with **automatic backend
selection**:

- **GCC / Clang** — native `__int128`. No external dependencies.
- **MSVC** — `boost::multiprecision::checked_int128_t` (header-only, no linking).

Backend selection happens automatically in `fixed128.hpp` based on
`__SIZEOF_INT128__`. Define `FIXED128_FORCE_BOOST_BACKEND` before including
to force the Boost backend even on GCC/Clang (useful for cross-platform
testing).

## Project structure

~~~
Fixed128/
    CMakeLists.txt
    README.md
    include/fixed128/
        fixed128.hpp              # public API + class (backend-agnostic)
        detail/
            backend_gcc.hpp       # __int128 backend (no Boost)
            backend_msvc.hpp      # Boost.Multiprecision backend (MSVC)
    tests/
        CMakeLists.txt
        test_fixed128.cpp
~~~

## Features

- `Fixed128<MaxPrecision>` — 128-bit mantissa, `int8_t` exponent
- Full arithmetic: `+`, `-`, `*`, `/`, unary `-`, unary `+`
- Compound assignment: `+=`, `-=`, `*=`, `/=`
- Increment / decrement: `++`, `--` (pre and post)
- Full comparison: `==`, `!=`, `<`, `<=`, `>`, `>=` (cross-exponent safe)
- `rescale(target, mode)`, `round(target, mode)` — all rounding modes
- `round_half_even`, `round_half_up` — convenience wrappers
- `normalize()` — strips trailing zeros
- `divide_at(divisor, target)` — explicit target (bypasses `MaxPrecision`)
- `to_string()`, `operator<<`

## Requirements

- C++14 or newer
- CMake >= 3.14
- Boost headers **only on MSVC** (header-only; no linking)
- Google Test (fetched automatically for tests)

## Build

~~~bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
~~~

CMake detects whether `__int128` is natively available; only if not, it
looks for Boost.

## Usage

~~~cpp
#include <fixed128/fixed128.hpp>
using namespace fixed128;

Fixed128<16> a(150000000LL, 8);   // 1.5
Fixed128<16> b(200000000LL, 8);   // 2.0
Fixed128<16> c(300000000LL, 8);   // 3.0

auto product  = a * b;                          // exp 16
auto quotient = (a * b) / c;                    // exp 16
auto negative = -a;                             // -1.5
auto scaled   = quotient.rescale(2, RoundingMode::HalfEven);

if (quotient == Fixed128<16>(1LL, 0)) { /* ... */ }
++a;                                            // a becomes 2.5
--b;                                            // b becomes 1.0
~~~

## Semantics

### MaxPrecision

Mandatory NTTP controlling derived operations:

- **`*`**: natural = `ea + eb`; capped at `MaxPrecision` (truncation).
- **`/`**: target = `MaxPrecision` **always**.

`+`, `-`, `rescale`, `divide_at`, unary operators, `++`, `--`, and
comparison do **not** apply `MaxPrecision`.

### Comparison

Compares **real values** across differing exponents:

~~~cpp
Fixed128<16>(15LL, 1) == Fixed128<16>(150LL, 2);   // true: both 1.5
Fixed128<16>(1LL, 0)  <  Fixed128<16>(15LL, 1);    // true: 1 < 1.5
~~~

- GCC backend: uses `__builtin_mul_overflow` to detect scaling overflow;
  falls back to sign-only reasoning when needed.
- MSVC backend: uses `boost::multiprecision::cpp_int` (arbitrary precision).

### Increment / decrement

`++x` / `--x` add / subtract **1.0**. Result exponent = `max(x.exponent, 0)`,
matching the addition rule.

### Rounding

~~~cpp
enum class RoundingMode {
    Truncate, Floor, Ceiling,
    HalfUp, HalfDown, HalfEven, HalfFloor, HalfCeiling
};
~~~

`HalfUp` — ties away from zero. `HalfEven` — bankers' rounding.

### normalize

Strips trailing zeros from the mantissa. Zero canonicalized to `(0, 0)`.

## Known limitations

1. `to_string()` supports only `exponent >= 0` (financial use case).
2. `operator*` may throw `overflow_error` on chained multiplications near
   the 128-bit limit (phantom overflow).
3. `operator/` with `MaxPrecision` greater than ~19–20 risks overflow.
   Use `divide_at` with a smaller target if needed.
4. Unary `-` on the minimum 128-bit value throws `overflow_error`.
