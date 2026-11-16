# fixed128

Header-only C++ fixed-point arithmetic library built on `__int128`.

## Features

- `Fixed128<MaxPrecision>` — `__int128` mantissa, `int8_t` exponent
- `operator+`, `operator-` — common denominator `max(ea, eb)`, no cap
- `operator*` — natural = `ea + eb`, capped at `MaxPrecision`
- `operator/` — natural target = `max(ea, eb)`, capped at `MaxPrecision`
- `+=`, `-=`, `*=`, `/=`
- `rescale(target, mode)`, `round(target, mode)` — all rounding modes
- `round_half_even`, `round_half_up` — convenience wrappers
- `normalize()` — strips trailing zeros; zero canonicalized to `(0, 0)`
- `divide_at(divisor, target)` — explicit target (not capped)
- `to_string()`, `operator<<`

## Requirements

- C++17
- GCC >= 5.1 or Clang >= 3.8
- CMake >= 3.14

## Build

~~~bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
~~~

## Usage

`MaxPrecision` is a **mandatory** template parameter. Choose a value
appropriate for your use case (e.g. `16` for financial, `127` for
effectively unbounded).

~~~cpp
#include <fixed128/fixed128.hpp>
using namespace fixed128;

Fixed128<16> x(150000000LL, 8);   // 1.5
Fixed128<16> y(200000000LL, 8);   // 2.0
Fixed128<16> z(300000000LL, 8);   // 3.0

auto r = (x * y) / z;                                 // 1.0 at exp 16
auto rounded = r.rescale(2, RoundingMode::HalfEven);  // 1.00
auto canon   = Fixed128<127>(1500, 3).normalize();    // 15 at exp 1
~~~

## Semantics

### MaxPrecision

`MaxPrecision` (mandatory NTTP) limits the automatic target exponent
of `*` and `/` *after* the natural target is derived:

- `*`: natural = `ea + eb`, then `min(natural, MaxPrecision)`. If capping
  occurs, the mantissa is divided by `10^(natural - MaxPrecision)` (truncation).
- `/`: natural = `max(ea, eb)`, then `min(natural, MaxPrecision)`.

`+`, `-`, `rescale`, `divide_at`, and the constructor do **not** apply
the cap — they represent explicit operations, not derived ones.

### Division target

Division uses `max(ea, eb)` as natural target, not `ea`. Rationale:

- For operands with exponents `ea = 16` (e.g. result of a prior
  multiplication) and `eb = 8`, the target is `max(16, 8) = 16` —
  the intended precision of a chained operation.
- For operands with exponents `ea = 8`, `eb = 4`, the target is `8` —
  preserves the better operand.
- Multiplication-style `ea + eb` is *not* used: it would create false
  precision and often overflow `__int128`.

### Rounding

~~~cpp
enum class RoundingMode {
    Truncate, Floor, Ceiling,
    HalfUp, HalfDown, HalfEven, HalfFloor, HalfCeiling
};
~~~

- `HalfUp` — ties away from zero (like `std::round`)
- `HalfEven` — bankers' rounding (ties to even)
- `rescale(target, mode)` applies `mode` only on downscale

### normalize

Strips trailing zeros from the mantissa, reducing the exponent down to
(but not below) zero. Zero is canonicalized to `(0, 0)`.

## Known limitations

1. `to_string()` supports only `exponent >= 0` (financial use case).
2. `compute_fraction` may throw `overflow_error` for mantissas
   `> INT128_MAX / 10` with large positive scaling shift.
3. `int128` value limit at `exponent = 16`: `|value| < 1.7e22`.
4. `operator*` computes the raw mantissa product before applying
   `MaxPrecision`. For int64_t-mantissa inputs (|m| < 2^63) the product
   is < 2^126 and always fits. For chained `*` on already-scaled values
   near `INT128_MAX`, the product may overflow and throw even if the
   capped result would fit — a standard "phantom overflow" trade-off.
