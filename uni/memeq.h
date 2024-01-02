#pragma once
#include <cstring>
#include "common.h"

namespace uni {

 // Like memcmp but only tests equality.  We have to manually vectorize this
 // because the compiler can't vectorize loops with sequential memory accesses
 // (it doesn't know when the memory will become invalid).
 //
 // Use memcmp instead of this when...
 //   - ...the size is likely to be large.  I'd guess memcmp starts to win at
 //     around a hundred or two hundred bytes or so.
 //   - ...the size is known at compile-time.  If the size is small and known at
 //     compile-time, then the compiler can optimize memcmp into very small and
 //     fast code.
 //   - ...you think you'll be running on hardward where misaligned access is
 //     illegal or very slow.  This should still work correctly, but memcmp will
 //     be faster.  TODO: detect those platforms with a huge mess of macros?
NOINLINE constexpr bool memeq (const void* a, const void* b, usize s) {
    if (std::is_constant_evaluated()) {
         // The algorithm below can't be constexpr because of reinterpret_casts,
         // so use memcmp which gets special treatment to be constexpr.
        return std::memcmp(a, b, s) == 0;
    }
    auto ap = (const char*)a;
    auto bp = (const char*)b;
     // Modern CPUs can do 16 or more bytes at a time, but I can't coax the
     // compiler into doing that, and it probably isn't really worth it for the
     // sizes of strings we're likely to be comparing.
    if (s >= 8) {
        auto ae = ap + s - 8;
        auto be = bp + s - 8;
        for (; ap < ae; ap += 8, bp += 8) {
             // We're still using memcmp internally because making misaligned
             // pointers is UB in C and C++.  This compiles to exactly the same
             // code as *(uint64*)ap != *(uint64*)bp.
            if (std::memcmp(ap, bp, 8) != 0) return false;
        }
        return std::memcmp(ae, be, 8) == 0;
    }
    else if (s >= 4) {
         // In the case of s == 4, these reads will overlap exactly, but it's
         // not worth doing another branch to check for that.
        return std::memcmp(ap, bp, 4) == 0 &&
               std::memcmp(ap + s - 4, bp + s - 4, 4) == 0;
    }
    else {
         // This unrolls better than the pointer version, and unrolling three
         // iterations doesn't take much more space than the original loop.
         // Also the compiler unrolls it better than I can manually.
        for (usize i = 0; i < s; i++) {
            if (ap[i] != bp[i]) return false;
        }
        return true;
    }
}
 // This seems to be significantly faster than memcmp for shortish strings,
 // according to my (highly sketchy) measurements.  Digging through my system's
 // header files, I also found __memcmpeq, but not only is there a comment
 // saying not to call it manually, it ended up being slower than memcmp.
 // __memcmpeq seems to use AVX2 (32-byte vectors) on my system, which is way
 // overkill for shortish strings, whereas memcmp just uses SSE2 (16-byte
 // vectors).  It might end up being faster at a few hundred bytes or so, but
 // who knows.

} // uni
