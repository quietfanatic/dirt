#pragma once
#include "common.h"
#include <cstring>

namespace uni {

namespace in {
bool memeq_internal (const char* a, const char* b, usize s);
const char* mem_first_difference_internal (const char* a, const char* b, const char* ae);
} // in

 // Like memcmp but only tests equality.  This is faster than a simple
 // byte-comparison loop (which the compiler can't vectorize because it reads
 // variable amounts of memory) and is usually faster than memcmp.
 //
 // Use memcmp instead of this when...
 //   - ...the size is likely to be large.  I haven't measured the threshold but
 //     I'd guess memcmp starts to win at around one or two hundred bytes or so,
 //     possibly earlier if the pointers aren't aligned.
 //   - ...the size is known at compile-time.  If the size is small and known at
 //     compile-time, then the compiler will (hopefully) optimize memcmp into
 //     very small and fast code.
 //   - ...you're suspicious of amateur memory-manipulation code compared to
 //     tried-and-true standard library code.
 //   - ...portability is more important than performance.
 // In other words, use memcmp instead of this. :3
 //
 // Unlike memcmp, you are allowed to pass nullptrs if s == 0.  Most memcmp
 // implementations allow this but the language spec does not.
 //
 // Also, be aware that this returns true on equality and false on inequality,
 // the opposite of memcmp.



ALWAYS_INLINE constexpr
bool memeq (const void* a, const void* b, usize s) {
     // Only run this on architectures that we know have reasonably fast
     // misaligned access.
#if defined(__amd64__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64)
     // The ordinary algorithm can't be constexpr because of reinterpret_casts,
     // but std::memcmp gets special treatment to be constexpr.
    if consteval {
#else
    if (true) {
#endif
        return s == 0 || std::memcmp(a, b, s) == 0;
    }
    else {
        return in::memeq_internal((const char*)a, (const char*)b, s);
    }
}

 // Get the index of the first character that's different between the two
 // strings.
ALWAYS_INLINE constexpr
usize mem_first_difference (const char* a, const char* b, usize s) {
#if defined(__amd64__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64)
    if consteval {
#else
    if (true) {
#endif
        for (usize i = 0; i < s && a[i] == b[i]; i++);
        return s;
    }
    else {
        return in::mem_first_difference_internal(a, b, a + s) - a;
    }
}

} // uni
