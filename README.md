# Fixed128

Header-only C++ fixed-point arithmetic library with **automatic backend
selection**:

- **GCC / Clang** — native `__int128`. No external dependencies.
- **MSVC** — `boost::multiprecision` checked types (header-only, no linking).

Backend is selected in `fixed128.hpp` based on `__SIZEOF_INT128__`. Define
`FIXED128_FORCE_BOOST_BACKEND` to force the Boost backend on any compiler
(useful for cross-platform tests).

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
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=` (cross-exponent safe)
- Comparison and `+=` / `-=` with `int64_t` (treats `int64_t` as exp-0 value)
- `rescale(target, mode)`, `round(target, mode)` — all rounding modes
- `round_half_even`, `round_half_up` — convenience wrappers
- `normalize()` — strips trailing zeros
- `divide_at(divisor, target)` — explicit target (bypasses `MaxPrecision`)
- `from_parts(int128_t, int8_t)`, `raw_mantissa()`, `raw_exponent()` —
  read access without leaking raw arithmetic overflow behaviour
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

CMake detects native `__int128` and requires Boost only when it's absent.

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

a += 1;                                         // 2.5
if (quotient != 1) { /* ... */ }                // integer comparison

auto rounded  = quotient.rescale(2, RoundingMode::HalfEven);
auto canon    = Fixed128<16>(1500, 3).normalize();

auto big = Fixed128<16>::from_parts(
    fixed128::int128_t(1234567890123456789LL), 12);
auto m = big.raw_mantissa();
auto e = big.raw_exponent();
~~~

## Semantics

### MaxPrecision

Mandatory NTTP controlling derived operations:

- **`*`**: natural = `ea + eb`; capped at `MaxPrecision` (truncation toward
  zero). When the capping down-shift exceeds 38, the truncated result is
  mathematically 0 and the operator returns `(0, MaxPrecision)` — no throw.
- **`/`**: target = `MaxPrecision` **always**.

`+`, `-`, `rescale`, `divide_at`, unary operators, `++`, `--`, and
comparison do **not** apply `MaxPrecision`.

### Comparison

Compares **real values** across differing exponents:

~~~cpp
Fixed128<16>(15LL, 1) == Fixed128<16>(150LL, 2);   // true: both 1.5
Fixed128<16>(1LL, 0)  <  Fixed128<16>(15LL, 1);    // true: 1 < 1.5
~~~

`int64_t` operands are implicitly interpreted as exp-0 values for `==` / `!=`
and `+=` / `-=`.

### Increment / decrement

`++x` / `--x` add / subtract **1.0**. Result exponent = `max(x.exponent, 0)`.

### Rounding

~~~cpp
enum class RoundingMode {
    Truncate, Floor, Ceiling,
    HalfUp, HalfDown, HalfEven, HalfFloor, HalfCeiling
};
~~~

`HalfUp` — ties away from zero. `HalfEven` — bankers' rounding.

### normalize

Strips trailing zeros from the mantissa; zero is canonicalised to `(0, 0)`.

### Encapsulation

`mantissa` and `exponent` are private. Use `raw_mantissa()` / `raw_exponent()`
for read access and `from_parts(...)` to construct from a 128-bit value.
This prevents leaking the difference between backends: on GCC, mutating a
public `__int128` field could silently overflow (UB); on MSVC the same code
would throw.

## Known limitations

1. `to_string()` supports only `exponent >= 0`.
2. `operator+` / `operator-`: if the exponent gap is very large, the
   upscale of the smaller operand may overflow `int128_t` and
   `std::overflow_error` is thrown even when the mathematical result would
   fit. Use `rescale()` for explicit control.
3. `operator*`: phantom overflow — the raw product is computed before the
   `MaxPrecision` cap. For `int64_t`-mantissa inputs (|m| < 2^63) the
   product is < 2^126 and always fits.
4. `operator/`: `MaxPrecision ≤ 38 − |ea − eb|` (pow10 guard). For typical
   financial inputs (ea = eb) `MaxPrecision ≤ 38` is safe; values above
   ~19–20 combined with already-scaled numerators risk overflow. Use
   `divide_at` with a smaller target if needed.
5. `compute_fraction` (division's fractional part) can still overflow when
   `|divisor| > INT128_MAX / 10` with a large positive `scaling_shift`;
   documented as a numeric limit of 128-bit arithmetic.
6. Unary `-` on the minimum 128-bit value throws `overflow_error`.
