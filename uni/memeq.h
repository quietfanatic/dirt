#pragma once
#include <cstring>
#include "common.h"

namespace uni {

 // Like memcmp but only tests equality.  We have to manually vectorize this
 // because the compiler can't vectorize loops with sequential memory accesses
 // (it doesn't know when the memory will become invalid).
 //
 // This requires that either the pointers are 8-byte-aligned or the CPU
 // supports misaligned access without too much performance penalty.  I believe
 // this is the case for most modern CPUs.
 //
 // My (very cursory) measurements indicate this is faster than using memcmp
 // (which is itself faster than trying to use __memcmpeq), at least for
 // relatively short strings.
NOINLINE constexpr bool memeq (const void* a, const void* b, usize s) {
    if (std::is_constant_evaluated()) {
         // memcmp gets special treatment for being constexpr
        return std::memcmp(a, b, s) == 0;
    }
    auto ap = (const char*)a;
    auto bp = (const char*)b;
     // Modern CPUs can do 16 or more bytes at a time, but it's difficult to
     // make the compiler do this portably (__int128 doesn't work) and it's
     // unlikely to be that helpful for common string lengths.
    if (s >= 8) {
        auto ae = ap + s - 8;
        auto be = bp + s - 8;
        for (; ap < ae; ap += 8, bp += 8) {
            if (*(uint64*)ap != *(uint64*)bp) return false;
        }
        return *(uint64*)ae == *(uint64*)be;
    }
    else if (s >= 4) {
        return *(uint32*)ap == *(uint32*)bp &&
               *(uint32*)(ap + s - 4) == *(uint32*)(bp + s - 4);
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

} // uni
