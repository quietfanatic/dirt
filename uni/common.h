#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace uni {
inline namespace common {

///// New-fangled int names
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using isize = std::intptr_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::uintptr_t;
 // I once met a compiler that defined uint32_t as unsigned int, uint64_t as
 // unsigned long long, but size_t as unsigned long.  If that happens, fiddle
 // with the compiler flags until it doesn't.
static_assert(std::is_same_v<isize, i32> || std::is_same_v<isize, i64>);
static_assert(std::is_same_v<usize, u32> || std::is_same_v<usize, u64>);
static_assert(std::is_same_v<usize, std::size_t>);

///// Old-fashioned int names
using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;
using uint = unsigned int;  // 32 bits on most platforms
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

///// Other types and constants
 // Anyone using this header is expected to treat char as UTF-8.
 // These are not in std:: for some reason
using char16 = char16_t;
using char32 = char32_t;

using Null = std::nullptr_t;
constexpr Null null = nullptr;

#ifdef nan
    #warning "Somebody defined nan as a macro, undefining it"
    #undef nan
#endif
constexpr float nan = std::numeric_limits<float>::quiet_NaN();
#ifdef inf
    #warning "Somebody defined inf as a macro, undefining it"
    #undef inf
#endif
constexpr float inf = std::numeric_limits<float>::infinity();

using std::move;

} // common
} // uni

///// MACROS

#ifndef HAS_BUILTIN
    #ifdef __has_builtin
        #define HAS_BUILTIN(f) __has_builtin(f)
    #else
        #define HAS_BUILTIN(f)
    #endif
#endif

 // These need to be before static and constexpr
#ifndef ALWAYS_INLINE
    #if __GNUC__
        #define ALWAYS_INLINE [[gnu::always_inline]] inline
    #elif _MSC_VER
        #define ALWAYS_INLINE __forceinline
    #else
        #define ALWAYS_INLINE inline
    #endif
#endif

#ifndef NOINLINE
    #if __GNUC__
        #define NOINLINE [[gnu::noinline]]
    #elif _MSC_VER
        #define NOINLINE __declspec(noinline)
    #else
        #define NOINLINE
    #endif
#endif

 // TODO: DECLARE_ENUM_ARITHMETIC_OPERATORS
#ifndef DECLARE_ENUM_BITWISE_OPERATORS
#define DECLARE_ENUM_BITWISE_OPERATORS(T) \
ALWAYS_INLINE constexpr T operator | (T a, T b) { \
    return T(std::underlying_type_t<T>(a) | std::underlying_type_t<T>(b)); \
} \
ALWAYS_INLINE constexpr T& operator |= (T& a, T b) { \
    return a = T(std::underlying_type_t<T>(a) | std::underlying_type_t<T>(b)); \
} \
ALWAYS_INLINE constexpr T operator & (T a, T b) { \
    return T(std::underlying_type_t<T>(a) & std::underlying_type_t<T>(b)); \
} \
ALWAYS_INLINE constexpr T& operator &= (T& a, T b) { \
    return a = T(std::underlying_type_t<T>(a) & std::underlying_type_t<T>(b)); \
} \
ALWAYS_INLINE constexpr T operator ^ (T a, T b) { \
    return T(std::underlying_type_t<T>(a) ^ std::underlying_type_t<T>(b)); \
} \
ALWAYS_INLINE constexpr T& operator ^= (T& a, T b) { \
    return a = T(std::underlying_type_t<T>(a) ^ std::underlying_type_t<T>(b)); \
} \
ALWAYS_INLINE constexpr T operator ~ (T a) { \
    return T(~std::underlying_type_t<T>(a)); \
} \
ALWAYS_INLINE constexpr bool operator ! (T a) { \
    return !std::underlying_type_t<T>(a); \
} \
ALWAYS_INLINE constexpr T operator << (T a, int b) { \
    return T(std::underlying_type_t<T>(a) << b); \
} \
ALWAYS_INLINE constexpr T& operator <<= (T& a, int b) { \
    return a = T(std::underlying_type_t<T>(a) << b); \
} \
ALWAYS_INLINE constexpr T operator >> (T a, int b) { \
    return T(std::underlying_type_t<T>(a) >> b); \
} \
ALWAYS_INLINE constexpr T& operator >>= (T& a, int b) { \
    return a = T(std::underlying_type_t<T>(a) >> b); \
} \
/* There's no way to get enum classes to boolify themselves, and looking at */ \
/* !!(flags & Flags::Flag) all the time is tedious so here's some relief. */ \
/* I tried operator / too but it looks too much like |. */ \
ALWAYS_INLINE constexpr bool operator % (T a, T b) { \
    return !!(std::underlying_type_t<T>(a) & std::underlying_type_t<T>(b)); \
}
#endif
