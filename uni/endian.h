#include "common.h"

namespace uni {

constexpr
u64 read_u64le (const void* in) {
    u64 v;
    std::memcpy(&v, in, 8);
    return v;
}

constexpr
u64 read_u64be (const void* in) {
#if HAS_BUILTIN(__builtin_bswap64)
     // Even if the hardware allows it, misaligned access is UB in the language
     // spec.  This has caused problems before.
    u64 v;
    std::memcpy(&v, in, 8);
    return __builtin_bswap64(v);
#else
    auto p = (const u8*)in;
    return u64(p[0]) << 56
         | u64(p[1]) << 48
         | u64(p[2]) << 40
         | u64(p[3]) << 32
         | u64(p[4]) << 24
         | u64(p[5]) << 16
         | u64(p[6]) << 8
         | u64(p[7]);
#endif
}

constexpr
void write_u64be (void* out, u64 v) {
#if HAS_BUILTIN(__builtin_bswap64)
    v = __builtin_bswap64(v);
    std::memcpy(out, &v, 8);
#else
    auto p = (u8*)out;
    p[0] = v >> 56;
    p[1] = v >> 48;
    p[2] = v >> 40;
    p[3] = v >> 32;
    p[4] = v >> 24;
    p[5] = v >> 16;
    p[6] = v >> 8;
    p[7] = v;
#endif
}

constexpr
u32 read_u32le (const void* in) {
    u32 v;
    std::memcpy(&v, in, 4);
    return v;
}

constexpr
u32 read_u32be (const void* in) {
#if HAS_BUILTIN(__builtin_bswap32)
    u32 v;
    std::memcpy(&v, in, 4);
    return __builtin_bswap32(v);
#else
    auto p = (const u8*)in;
    return u32(p[0]) << 24
         | u32(p[1]) << 16
         | u32(p[2]) << 8
         | u32(p[3]);
#endif
}

constexpr
void write_u32be (void* out, u64 v) {
#if HAS_BUILTIN(__builtin_bswap64)
    v = __builtin_bswap32(v);
    std::memcpy(out, &v, 4);
#else
    auto p = (u8*)out;
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
#endif
}

constexpr
u16 read_u16le (const void* in) {
    u16 v;
    std::memcpy(&v, in, 2);
    return v;
}

constexpr
u16 read_u16be (const void* in) {
#if HAS_BUILTIN(__builtin_bswap16)
    u16 v;
    std::memcpy(&v, in, 2);
    return __builtin_bswap16(v);
#else
    auto p = (const u8*)in;
    return p[0] << 8 | p[1];
#endif
}

constexpr
void write_u16be (void* out, u16 v) {
    auto p = (u8*)out;
    p[0] = v >> 8;
    p[1] = v;
}

constexpr i64 read_i64le (const void* in) { return read_u64le(in); }
constexpr i64 read_i64be (const void* in) { return read_u64be(in); }
constexpr i64 read_i32le (const void* in) { return read_u32le(in); }
constexpr i64 read_i32be (const void* in) { return read_u32be(in); }
constexpr i64 read_i16le (const void* in) { return read_u16le(in); }
constexpr i64 read_i16be (const void* in) { return read_u16be(in); }
constexpr void write_i64be (void* out, i16 v) { write_i64be(out, v); }
constexpr void write_i32be (void* out, i16 v) { write_i32be(out, v); }
constexpr void write_i16be (void* out, i16 v) { write_i16be(out, v); }

} // uni

