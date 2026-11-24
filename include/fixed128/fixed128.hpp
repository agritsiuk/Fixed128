#pragma once
// =============================================================================
// Fixed128 — single public entry point.
//
// Backend selection (automatic):
//   - GCC / Clang (__SIZEOF_INT128__) -> full backend (fixed128_full.hpp)
//   - MSVC                            -> lite backend (fixed128_lite.hpp)
//   - FIXED128_FORCE_LITE_BACKEND     -> lite backend everywhere
//
// "full" provides: full arithmetic, comparisons, rescale/round, hash.
// "lite" provides: from_string, to_string, normalize, from_parts, raw accessors.
// =============================================================================

#if defined(FIXED128_FORCE_LITE_BACKEND)
    #include "fixed128_lite.hpp"
#elif defined(__SIZEOF_INT128__)
    #include "fixed128_full.hpp"
#elif defined(_MSC_VER)
    #include "fixed128_lite.hpp"
#else
    #error "fixed128: unsupported compiler (need __int128 support or MSVC)"
#endif
