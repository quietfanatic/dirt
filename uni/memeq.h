#pragma once
#include <cstring> // memcpy

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

constexpr
bool memeq (const void* a, const void* b, std::size_t s) {
     // Only run this on architectures that we know have reasonably fast
     // misaligned access.
#if defined(__amd64__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64)
     // The algorithm below can't be constexpr because of reinterpret_casts,
     // but std::memcmp gets special treatment to be constexpr.
    bool dont_do_it = std::is_constant_evaluated();
#else
    bool dont_do_it = true;
#endif
    if (dont_do_it) {
        return s == 0 || std::memcmp(a, b, s) == 0;
    }

    auto ap = (const char*)a;
    auto bp = (const char*)b;
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
        auto ae = ap + s - 8;
        u64 av;
        u64 bv;
        do {
            std::memcpy(&av, ap, 8);
            std::memcpy(&bv, bp, 8);
            if (av != bv) return false;
            ap += 8;
            bp += 8;
        } while (ap < ae);
         // Finish off with the possibly overlapping final eight bytes.
         // Do a little math to save one register.
        std::memcpy(&av, ae, 8);
        std::memcpy(&bv, bp + (ae - ap), 8);
        return av == bv;
    }
    else if (s >= 4) {
         // In the case of s == 4, these reads will overlap exactly, but
         // it's not worth doing another branch to check for that.
        u32 av;
        u32 bv;
         // Do the more complex addressing first so s can be retired
        std::memcpy(&av, ap + s - 4, 4);
        std::memcpy(&bv, bp + s - 4, 4);
        u32 x = av ^ bv;
        std::memcpy(&av, ap, 4);
        std::memcpy(&bv, bp, 4);
        return !(x | (av ^ bv));
    }
    else [[unlikely]] if (s) {
         // There isn't really anything satisfying to do here.  We're
         // prioritizing the speed of the longer strings, so here we're more
         // concerned with register usage, code size, and branch predictor
         // pressure than speed.
        //for (u32 i = 0; i < s; i++) {
        //    if (ap[i] != bp[i]) return false;
        //}
        //return true;
        if (s >= 2) {
            u16 av;
            u16 bv;
            std::memcpy(&av, ap + s - 2, 2);
            std::memcpy(&bv, bp + s - 2, 2);
            if (av != bv) return false;
        }
        return ap[0] == bp[0];
         // Here's a cool branchless version.  Unfortunately it tends to use
         // more registers and compiles larger.
        //return !((ap[s-1] ^ bp[s-1])
        //       | (ap[s>>1] ^ bp[s>>1])
        //       | (ap[0] ^ bp[0]));
    }
    else return true;
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
