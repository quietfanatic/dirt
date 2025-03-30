#pragma once

#include "common.h"

namespace glow {

 // A type representing a color with 32bpp depth.  If you want to use floats,
 // you should probably just use a Vec4.
struct RGBA8 {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    constexpr RGBA8 (u8 r, u8 g, u8 b, u8 a) :
        r(r), g(g), b(b), a(a)
    { }
     // Convert from a u32 in 0xRRGGBBAA format (native endian)
    constexpr RGBA8 (u32 rgba = 0) :
        r(rgba >> 24), g(rgba >> 16), b(rgba >> 8), a(rgba)
    { }
    constexpr explicit operator u32 () const {
        return u32(r) << 24 | u32(g) << 16 | u32(b) << 8 | u32(a);
    }
     // operator bool only checks alpha.
    constexpr explicit operator bool () const { return a; }
};
inline bool operator == (RGBA8 a, RGBA8 b) {
    return u32(a) == u32(b);
}

} // namespace glow

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"
namespace tap {
    template <>
    struct Show<glow::RGBA8> {
        uni::UniqueString show (const glow::RGBA8& v) {
            return uni::cat("RGBA(", v.r, ", ", v.g, ", ", v.b, ", ", v.a, ')');
        }
    };
}
#endif
