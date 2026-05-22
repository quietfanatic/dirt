#include "mem.h"

#include <bit>
#include "assertions.h"

namespace uni {

 // This seems to be significantly faster than memcmp for shortish strings,
 // according to my (highly sketchy) measurements.  Digging through my system's
 // header files, I also found __memcmpeq, but not only is there a comment
 // saying not to call it manually, it ended up being slower than memcmp.
 // __memcmpeq seems to use AVX (32-byte vectors) on my system, which is way
 // overkill for shortish strings, whereas memcmp just uses SSE (16-byte
 // vectors).  It might end up being faster at a few hundred bytes or so, but
 // who knows.
NOINLINE
bool in::memeq_internal (const char* a, const char* b, usize s) {
     // Branch at 9 instead of 8.  While sending 8 through the 4+4 path sounds
     // unoptimal, if we allow 8 into the 8-by-8 path, the compiler will add an
     // extra branch to skip the loop when s is exactly 8.
    if (s >= 9) {
         // Read eight bytes at a time.  Modern X64 CPUs have instructions to
         // work with 16 or even 32 bytes at a time, but we won't use them
         // because:
         //   - It can be tricky to convince the compiler to use them.
         //   - They'll require yet another codepath for 8-16 bytes.
         //   - They're overkill for common string lengths anyway.
        auto ae = a + s - 8;
        u64 av;
        u64 bv;
        #pragma GCC unroll 0
        #pragma GCC novector
        do {
            std::memcpy(&av, a, 8);
            std::memcpy(&bv, b, 8);
            if (av != bv) return false;
            a += 8;
            b += 8;
        } while (a < ae);
         // Finish off with the possibly overlapping final eight bytes.
        std::memcpy(&av, ae, 8);
        std::memcpy(&bv, b + (ae - a), 8);
        return av == bv;
    }
    else if (s >= 4) {
         // In the case of s == 4, these reads will overlap exactly, but
         // it's not worth doing another branch to check for that.
        u32 av;
        u32 bv;
         // Do the more complex addressing first so s can be retired
        std::memcpy(&av, a + s - 4, 4);
        std::memcpy(&bv, b + s - 4, 4);
        u32 x = av ^ bv;
        std::memcpy(&av, a, 4);
        std::memcpy(&bv, b, 4);
        return !(x | (av ^ bv));
    }
    else [[unlikely]] if (s) {
         // There isn't really anything satisfying to do here.  We're
         // prioritizing the speed of the longer strings, so here we're more
         // concerned with code size and branch predictor pressure than raw
         // speed.

         // Naive version
        if (0) {
            for (u32 i = 0; i < s; i++) {
                if (a[i] != b[i]) return false;
            }
            return true;
        }
         // Smallest version (for if we're inlined)
        if (0) {
            if (s >= 2) {
                u16 av;
                u16 bv;
                std::memcpy(&av, a + s - 2, 2);
                std::memcpy(&bv, b + s - 2, 2);
                if (av != bv) return false;
            }
            return a[0] == b[0];
        }
         // Branchless version (for noinline; problematic when inlined)
        return !((a[s-1] ^ b[s-1])
               | (a[s>>1] ^ b[s>>1])
               | (a[0] ^ b[0]));
    }
    else return true;
}

NOINLINE
const char* in::mem_first_difference_internal (const char* a, const char* b, const char* ae) {
    auto aem8 = ae - 8;
    if (a <= aem8) {
        u64 av;
        u64 bv;
        #pragma GCC novector // really not necessary.
        while (a < aem8) {
            std::memcpy(&av, a, 8);
            std::memcpy(&bv, b, 8);
            if (av != bv) goto find_byte;
            a += 8;
            b += 8;
        }
        b += aem8 - a;
        a = aem8;
        std::memcpy(&av, a, 8);
        std::memcpy(&bv, b, 8);
        if (av != bv) goto find_byte;
        return ae;

      find_byte:
        u32 same_bits;
        if constexpr (std::endian::native == std::endian::little) {
            same_bits = std::countr_zero(av ^ bv);
        }
        else {
            same_bits = std::countl_zero(av ^ bv);
        }
        assume(same_bits < 64);
        u32 same_bytes = same_bits >> 3;
        assume(a + same_bytes < ae);
        return a + same_bytes;
    }
    #pragma GCC unroll 0
    while (a < ae) {
        if (*a != *b) return a;
        a += 1;
        b += 1;
    }
    return ae;
}

} // uni
