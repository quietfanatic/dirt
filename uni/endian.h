#pragma once

// WARNING!  None of these functions are tested on anything other than
// little-endian GCC.

#include <bit>
#include "common.h"

namespace uni {

constexpr bool big_endian = std::endian::native == std::endian::big;
constexpr bool little_endian = std::endian::native == std::endian::little;

constexpr u64 byteswap_u64 (u64 v) {
#if __cpp_lib_byteswap
    return std::byteswap(v);
#elif HAS_BUILTIN(__builtin_bswap64)
    return __builtin_bswap64(v);
#elif _MSC_VER
    return _byteswap_uint64(v);
#else
    return (v >> 56)
         | ((v >> 40) & 0xff00)
         | ((v >> 24) & 0xff'0000)
         | ((v >> 8) & 0xff00'0000)
         | ((v << 8) & 0xff0000'0000)
         | ((v << 24) & 0xff00'0000'0000)
         | ((v << 40) & 0xff'0000'0000'0000)
         | (v << 56);
#endif
}

constexpr u32 byteswap_u32 (u32 v) {
#if __cpp_lib_byteswap
    return std::byteswap(v);
#elif HAS_BUILTIN(__builtin_bswap32)
    return __builtin_bswap32(v);
#elif _MSC_VER
    return _byteswap_ulong(v);
#else
    return (v >> 24)
         | ((v >> 8) & 0xff00)
         | ((v << 8) & 0xff0000)
         | (v << 24);
#endif
}

constexpr u16 byteswap_u16 (u16 v) {
#if __cpp_lib_byteswap
    return std::byteswap(v);
#elif HAS_BUILTIN(__builtin_bswap16)
    return __builtin_bswap16(v);
#elif _MSC_VER
    return _byteswap_ushort(v);
#else
    return (v >> 8) | (v << 8);
#endif
}

constexpr i64 byteswap_i64 (i64 v) { return i64(byteswap_u64(u64(v))); }
constexpr i32 byteswap_i32 (i32 v) { return i32(byteswap_u32(u32(v))); }
constexpr i16 byteswap_i16 (i16 v) { return i16(byteswap_u16(u16(v))); }

constexpr u64 read_u64ne (const void* in) { u64 v; std::memcpy(&v, in, 8); return v; }
constexpr i64 read_i64ne (const void* in) { i64 v; std::memcpy(&v, in, 8); return v; }
constexpr u32 read_u32ne (const void* in) { u32 v; std::memcpy(&v, in, 4); return v; }
constexpr i32 read_i32ne (const void* in) { i32 v; std::memcpy(&v, in, 4); return v; }
constexpr u16 read_u16ne (const void* in) { u16 v; std::memcpy(&v, in, 2); return v; }
constexpr i16 read_i16ne (const void* in) { i16 v; std::memcpy(&v, in, 2); return v; }
constexpr u64 read_u64re (const void* in) { u64 v; std::memcpy(&v, in, 8); return byteswap_u64(v); }
constexpr i64 read_i64re (const void* in) { i64 v; std::memcpy(&v, in, 8); return byteswap_i64(v); }
constexpr u32 read_u32re (const void* in) { u32 v; std::memcpy(&v, in, 4); return byteswap_u32(v); }
constexpr i32 read_i32re (const void* in) { i32 v; std::memcpy(&v, in, 4); return byteswap_i32(v); }
constexpr u16 read_u16re (const void* in) { u16 v; std::memcpy(&v, in, 2); return byteswap_u16(v); }
constexpr i16 read_i16re (const void* in) { i16 v; std::memcpy(&v, in, 2); return byteswap_i16(v); }

constexpr void write_u64ne (void* in, u64 v) { std::memcpy(in, &v, 8); }
constexpr void write_i64ne (void* in, i64 v) { std::memcpy(in, &v, 8); }
constexpr void write_u32ne (void* in, u32 v) { std::memcpy(in, &v, 4); }
constexpr void write_i32ne (void* in, i32 v) { std::memcpy(in, &v, 4); }
constexpr void write_u16ne (void* in, u16 v) { std::memcpy(in, &v, 2); }
constexpr void write_i16ne (void* in, i16 v) { std::memcpy(in, &v, 2); }
constexpr void write_u64re (void* in, u64 v) { v = byteswap_u64(v); std::memcpy(in, &v, 8); }
constexpr void write_i64re (void* in, i64 v) { v = byteswap_i64(v); std::memcpy(in, &v, 8); }
constexpr void write_u32re (void* in, u32 v) { v = byteswap_u32(v); std::memcpy(in, &v, 4); }
constexpr void write_i32re (void* in, i32 v) { v = byteswap_i32(v); std::memcpy(in, &v, 4); }
constexpr void write_u16re (void* in, u16 v) { v = byteswap_u16(v); std::memcpy(in, &v, 2); }
constexpr void write_i16re (void* in, i16 v) { v = byteswap_i16(v); std::memcpy(in, &v, 2); }

constexpr inline auto& read_u64le = big_endian ? read_u64re : read_u64ne;
constexpr inline auto& read_i64le = big_endian ? read_i64re : read_i64ne;
constexpr inline auto& read_u32le = big_endian ? read_u32re : read_u32ne;
constexpr inline auto& read_i32le = big_endian ? read_i32re : read_i32ne;
constexpr inline auto& read_u16le = big_endian ? read_u16re : read_u16ne;
constexpr inline auto& read_i16le = big_endian ? read_i16re : read_i16ne;
constexpr inline auto& read_u64be = big_endian ? read_u64ne : read_u64re;
constexpr inline auto& read_i64be = big_endian ? read_i64ne : read_i64re;
constexpr inline auto& read_u32be = big_endian ? read_u32ne : read_u32re;
constexpr inline auto& read_i32be = big_endian ? read_i32ne : read_i32re;
constexpr inline auto& read_u16be = big_endian ? read_u16ne : read_u16re;
constexpr inline auto& read_i16be = big_endian ? read_i16ne : read_i16re;
constexpr inline auto& write_u64le = big_endian ? write_u64re : write_u64ne;
constexpr inline auto& write_i64le = big_endian ? write_i64re : write_i64ne;
constexpr inline auto& write_u32le = big_endian ? write_u32re : write_u32ne;
constexpr inline auto& write_i32le = big_endian ? write_i32re : write_i32ne;
constexpr inline auto& write_u16le = big_endian ? write_u16re : write_u16ne;
constexpr inline auto& write_i16le = big_endian ? write_i16re : write_i16ne;
constexpr inline auto& write_u64be = big_endian ? write_u64ne : write_u64re;
constexpr inline auto& write_i64be = big_endian ? write_i64ne : write_i64re;
constexpr inline auto& write_u32be = big_endian ? write_u32ne : write_u32re;
constexpr inline auto& write_i32be = big_endian ? write_i32ne : write_i32re;
constexpr inline auto& write_u16be = big_endian ? write_u16ne : write_u16re;
constexpr inline auto& write_i16be = big_endian ? write_i16ne : write_i16re;

} // uni

