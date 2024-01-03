#pragma once
#include <cstring>

namespace uni {

 // Like memcmp but only tests equality.  This is faster than a simple
 // byte-comparison loop (which the compiler can't vectorize because it reads
 // variable amounts of memory) and is usually faster than memcmp.
 //
 // Use memcmp instead of this when...
 //   - ...the size is likely to be large.  I haven't measured the threshold but
 //     I'd guess memcmp starts to win at around one or two hundred bytes or so,
 //     possibly earlier if the pointers aren't aligned.
 //   - ...the size is known at compile-time.  If the size is small and known at
 //     compile-time, then the compiler will optimize memcmp into very small and
 //     fast code.
 //   - ...you're suspicious of amateur memory-manipulation code compared to
 //     tried-and-true standard library code.
 //   - ...portability is more important than performance.
 //
 // Unlike memcmp, you are allowed to pass nullptrs if s == 0.
constexpr bool memeq (const void* a, const void* b, std::size_t s) {
     // Only run this on architectures that we know support misaligned access.
#if defined(__amd64__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64)
    if (std::is_constant_evaluated()) {
         // The algorithm below can't be constexpr because of reinterpret_casts,
         // so use memcmp which gets special treatment to be constexpr.
        return std::memcmp(a, b, s) == 0;
    }
    else {
        auto ap = (const char*)a;
        auto bp = (const char*)b;
         // Modern X64 CPUs have instructions to work with 16 or evem 32 bytes
         // at a time, but they can have stricter alignment requirements, so the
         // compiler won't use them if the pointers aren't aligned.  They're
         // probably excessive for common string lengths anyway.
        if (s >= 8) {
             // Read eight bytes at a time
            auto ae = ap + s - 8;
            auto be = bp + s - 8;
            for (; ap < ae; ap += 8, bp += 8) {
                 // We're still using memcmp internally because misaligned
                 // pointers are UB in C and C++, even if the hardware doesn't
                 // care.  If the hardware supports misaligned reads, this
                 // compiles to exactly the same code as
                 // *(uint64*)ap != *(uint64*)bp.
                if (std::memcmp(ap, bp, 8) != 0) return false;
            }
             // Finish off with the possibly overlapping eight bytes.
            return std::memcmp(ae, be, 8) == 0;
        }
        else if (s >= 4) {
             // In the case of s == 4, these reads will overlap exactly, but
             // it's not worth doing another branch to check for that.
            return std::memcmp(ap, bp, 4) == 0 &&
                   std::memcmp(ap + s - 4, bp + s - 4, 4) == 0;
        }
         // Not worth doing comparisons of size 2
        else {
             // Indexing unrolls better than pointer-bumping, and unrolling
             // three iterations doesn't take much more space than the original
             // loop.  Also the compiler unrolls it better than I can manually.
            for (std::size_t i = 0; i < s; i++) {
                if (ap[i] != bp[i]) return false;
            }
            return true;
        }
    }
#else
     // Misaligned access may not be supported, so play it safe.
    return std::memcmp(a, b, s) == 0;
#endif
}
 // This seems to be significantly faster than memcmp for shortish strings,
 // according to my (highly sketchy) measurements.  Digging through my system's
 // header files, I also found __memcmpeq, but not only is there a comment
 // saying not to call it manually, it ended up being slower than memcmp.
 // __memcmpeq seems to use AVX (32-byte vectors) on my system, which is way
 // overkill for shortish strings, whereas memcmp just uses SSE (16-byte
 // vectors).  It might end up being faster at a few hundred bytes or so, but
 // who knows.

} // uni
