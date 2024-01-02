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
constexpr bool memeq (const void* a, const void* b, usize s) {
    if (std::is_constant_evaluated()) {
         // memcmp gets special treatment for being constexpr
        return std::memcmp(a, b, s) == 0;
    }
    auto ac = reinterpret_cast<const uint8*>(a);
    auto bc = reinterpret_cast<const uint8*>(b);
    auto ae = ac + s;
    while (ac + 8 <= ae) {
        if (*(uint64*)ac != *(uint64*)bc) return false;
        ac += 8;
        bc += 8;
    }
     // TODO: doing the tail with a misaligned 8-read should in theory be faster
     // but I'm getting conflicting measurements.  Actually measure it properly
     // at some point.
    for (; ac != ae; ac++, bc++) {
        if (*ac != *bc) return false;
    }
    return true;
}

} // uni
