# fixed128

Header-only C++ fixed-point arithmetic library built on `__int128`.

## Features

- `Fixed128<MaxPrecision>` — `__int128` mantissa, `int8_t` exponent
- `operator+`, `operator-` — common denominator `max(ea, eb)`, no cap
- `operator*` — natural = `ea + eb`, capped at `MaxPrecision`
- `operator/` — target **always** = `MaxPrecision` (inputs rescaled first)
- `+=`, `-=`, `*=`, `/=`
- `rescale(target, mode)`, `round(target, mode)` — all rounding modes
- `round_half_even`, `round_half_up` — convenience wrappers
- `normalize()` — strips trailing zeros; zero canonicalized to `(0, 0)`
- `divide_at(divisor, target)` — explicit target (bypasses `MaxPrecision`)
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
appropriate for your use case (e.g. `16` for financial data, `24` for
high precision). Large values (approaching 127) make division overflow
because the target scale `10^MaxPrecision` doesn't fit in `__int128`.

~~~cpp
#include <fixed128/fixed128.hpp>
using namespace fixed128;

Fixed128<16> x(150000000LL, 8);   // 1.5
Fixed128<16> y(200000000LL, 8);   // 2.0
Fixed128<16> z(300000000LL, 8);   // 3.0

auto r = (x * y) / z;                                 // exp 16
auto q = x / z;                                        // exp 16 (not 8!)
auto rounded = q.rescale(2, RoundingMode::HalfEven);   // exp 2
auto canon   = Fixed128<16>(1500, 3).normalize();      // exp 1
~~~

## Semantics

### MaxPrecision

`MaxPrecision` (mandatory NTTP) controls the exponent of derived
operations:

- **`*`**: natural = `ea + eb`; capped at `MaxPrecision`. If capping
  occurs, the mantissa is divided by `10^(natural - MaxPrecision)`
  (truncation toward zero).
- **`/`**: target = `MaxPrecision` **always**. Both operands are
  effectively rescaled to `MaxPrecision` before division, so the result
  exponent is always `MaxPrecision`, regardless of input exponents.

`+`, `-`, `rescale`, `divide_at`, and the constructor do **not** apply
`MaxPrecision` — they are explicit operations, not derived ones.

`divide_at(divisor, target)` provides an escape hatch: it uses `target`
directly, bypassing `MaxPrecision` entirely.

### Division target

Division always produces `MaxPrecision` decimal places. This means:

- `(x * y) / z` — the numerator already carries `ea + eb` (from `*`),
  and division lands on `MaxPrecision`. For `MaxPrecision = 16` and
  inputs at exp 8, the result has 16 digits after the decimal.
- `x / z` with both operands at exp 8 — result also has 16 digits.
  Division never "adds precision" from inputs; it uses `MaxPrecision`
  uniformly.
- The choice of `ea`, `eb` affects only the numerator (for `*`) or the
  internal scaling, never the final target.

If you need a different target, use `divide_at(divisor, target)`.

### Choosing MaxPrecision

- `MaxPrecision >= int64_t inputs' exponent` — sensible (no forced downscale).
- `MaxPrecision` in 2..18 — safe for financial data.
- `MaxPrecision` in 19..38 — possible but more overflow risk.
- `MaxPrecision > 38` — division almost always overflows `__int128`.

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
5. `operator/` with `MaxPrecision` greater than ~19–20 risks overflow
   because the numerator is effectively multiplied by `10^MaxPrecision`
   (via divmod). Use `divide_at` with a smaller target if needed.
