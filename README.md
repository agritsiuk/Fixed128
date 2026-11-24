# Fixed128

Header-only C++ fixed-point arithmetic. **No external dependencies**
(neither Boost nor any other library).

## Backend selection

Just `#include <fixed128/fixed128.hpp>`:

| Compiler            | Backend | Provided                                                        |
|---------------------|---------|-----------------------------------------------------------------|
| GCC / Clang         | full    | full arithmetic, comparisons, rounding, hash, parse/serialize   |
| MSVC                | lite    | `from_string` / `to_string` / `normalize` / raw accessors only  |
| any (opt-in)        | lite    | define `FIXED128_FORCE_LITE_BACKEND`                            |

## Features (full backend)

- `Fixed128<MaxPrecision>`: 128-bit mantissa + `int8_t` exponent
- Constructors: `(int64_t, int8_t)` and `(int128_t, int8_t)` (SFINAE-guarded)
- Arithmetic: `+ - * /`, unary `-`/`+`, `++`/`--`, compound assignment
- Comparisons: `== != < <= > >=`, plus `==`/`!=`/`+=`/`-=` with `int64_t`
- 10 rounding modes: `Truncate`, `Floor`, `Ceiling`, `AwayFromZero`,
  `HalfUp`, `HalfDown`, `HalfEven`, `HalfOdd`, `HalfFloor`, `HalfCeiling`
- `rescale`, `round`, `round_half_*`, `divide_at`
- `normalize`, `to_string`, `from_string`, `operator<<`
- `std::hash<Fixed128<MP>>` (consistent for non-negative exponents)

## Features (lite backend)

- `(int64_t, int8_t)` constructor + `from_parts(int128_t, int8_t)`
- `from_string` / `to_string` / `normalize` / `raw_*` accessors
- Full 128-bit mantissa (portable 4×`uint32_t` arithmetic for parse/serialize)

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
