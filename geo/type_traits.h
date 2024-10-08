// Using our own type traits system because std's type_traits is really sloppy
// about letting references through, and considers bools and chars to be
// integers, among other things.

#pragma once

#include <limits>
#include "common.h"

namespace geo {

template <class T>
struct TypeTraits {
    using Widened = T;
    static constexpr bool integral = false;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = false;
};
template <> struct TypeTraits<i8> {
    using Widened = i16;
    using MakeUnsigned = u8;
    static constexpr bool integral = true;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = true;
};
template <> struct TypeTraits<u8> {
    using Widened = u16;
    using MakeSigned = i8;
    static constexpr bool integral = true;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = false;
};
template <> struct TypeTraits<i16> {
    using Widened = i32;
    using MakeUnsigned = u16;
    static constexpr bool integral = true;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = true;
};
template <> struct TypeTraits<u16> {
    using Widened = u32;
    using MakeSigned = i16;
    static constexpr bool integral = true;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = false;
};
template <> struct TypeTraits<i32> {
    using Widened = i64;
    using MakeUnsigned = u32;
    static constexpr bool integral = true;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = true;
};
template <> struct TypeTraits<u32> {
    using Widened = u64;
    using MakeSigned = i32;
    static constexpr bool integral = true;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = false;
};
template <> struct TypeTraits<i64> {
    using Widened = i64;
    using MakeUnsigned = u64;
    static constexpr bool integral = true;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = true;
};
template <> struct TypeTraits<u64> {
    using Widened = u64;
    using MakeSigned = i64;
    static constexpr bool integral = true;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = false;
};
template <> struct TypeTraits<float> {
    using Widened = float;
    using SameSizeInt = i32;
    static constexpr bool integral = false;
    static constexpr bool floating = true;
    static constexpr bool fractional = true;
    static constexpr bool is_signed = true;
     // A number of functions in this library assume standard floating point
     // layout.
     // TODO: smallest normals
    static constexpr u32 SIGN_BIT = 0x8000'0000;
    static constexpr u32 EXPONENT_MASK = 0x7f80'0000;
    static constexpr float MINUS_INF = std::bit_cast<float>(0xff80'0000);
    static constexpr float MINUS_HUGE = std::bit_cast<float>(0xff7f'ffff);
    static constexpr float MINUS_TINY = std::bit_cast<float>(0x8000'0001);
    static constexpr float MINUS_ZERO = std::bit_cast<float>(0x8000'0000);
    static constexpr float PLUS_ZERO = std::bit_cast<float>(0x0000'0000);
    static constexpr float PLUS_TINY = std::bit_cast<float>(0x0000'0001);
    static constexpr float PLUS_HUGE = std::bit_cast<float>(0x7f7f'ffff);
    static constexpr float PLUS_INF = std::bit_cast<float>(0x7f80'0000);
    static_assert(std::numeric_limits<float>::infinity() == PLUS_INF);
    static_assert(std::numeric_limits<float>::max() == PLUS_HUGE);
    static_assert(std::numeric_limits<float>::lowest() == MINUS_HUGE);
    static_assert(std::numeric_limits<float>::denorm_min() == PLUS_TINY);
};
template <> struct TypeTraits<double> {
    using Widened = double;
    using SameSizeInt = i64;
    static constexpr bool integral = false;
    static constexpr bool floating = true;
    static constexpr bool fractional = true;
    static constexpr bool is_signed = true;
    static constexpr u64 SIGN_BIT = 0x8000'0000'0000'0000;
    static constexpr u64 EXPONENT_MASK = 0x7ff0'0000'0000'0000;
    static constexpr double MINUS_INF = std::bit_cast<double>(0xfff0'0000'0000'0000);
    static constexpr double MINUS_HUGE = std::bit_cast<double>(0xffef'ffff'ffff'ffff);
    static constexpr double MINUS_TINY = std::bit_cast<double>(0x8000'0000'0000'0001);
    static constexpr double MINUS_ZERO = std::bit_cast<double>(0x8000'0000'0000'0000);
    static constexpr double PLUS_ZERO = std::bit_cast<double>(u64(0x0000'0000'0000'0000));
    static constexpr double PLUS_TINY = std::bit_cast<double>(u64(0x0000'0000'0000'0001));
    static constexpr double PLUS_HUGE = std::bit_cast<double>(0x7fef'ffff'ffff'ffff);
    static constexpr double PLUS_INF = std::bit_cast<double>(0x7ff0'0000'0000'0000);
    static_assert(std::numeric_limits<double>::infinity() == PLUS_INF);
    static_assert(std::numeric_limits<double>::max() == PLUS_HUGE);
    static_assert(std::numeric_limits<double>::lowest() == MINUS_HUGE);
    static_assert(std::numeric_limits<double>::denorm_min() == PLUS_TINY);
};
 // long double is not supported by this library.

 // Whether this is an integral type.  It is expected that you can cast from
 // integer literals, pass by value, and do basic arithmetic operations.
template <class T>
concept Integral = TypeTraits<T>::integral;

template <class T>
concept SignedIntegral = Integral<T> && TypeTraits<T>::is_signed;

template <class T>
concept UnsignedIntegral = Integral<T> && !TypeTraits<T>::is_signed;

 // Get a wider version of the type for multiplication.  Does not widen floats,
 // doubles, or i64s.
template <class T>
using Widen = TypeTraits<T>::Widened;

template <class T>
constexpr Widen<T> widen (const T& v) { return v; }

template <class T>
using MakeUnsigned = TypeTraits<T>::MakeUnsigned;

template <class T>
using MakeSigned = TypeTraits<T>::MakeSigned;

 // Strictly floating point types (float, double) or types that can use the same
 // algorithms on them (with sign-exponent-mantissa representation).
template <class T>
concept Floating = TypeTraits<T>::floating;

 // Get an integer that's the same size as the given float.
template <Floating T>
using SameSizeInt = TypeTraits<T>::SameSizeInt;

 // Types that can store numbers inbetween 0 and 1, not necessarily floating
 // point (though currently this library doesn't provide any fractional
 // non-floating types).  It is expected that fractional numbers can be cast
 // from integers, passed by value, and have basic arithmetic operations as well
 // as trunc, round, etc.
template <class T>
concept Fractional = TypeTraits<T>::fractional;

 // Captures pointer-like types
template <class T>
concept Pointing = requires (T p) {
    *p; p[0]; p+1; ++p; p++; p-1; --p; p--; p - p; p == p; p != p;
};

 // Exact equality for everything but floats
template <class T> requires (!Floating<std::remove_cvref_t<T>>)
bool exact_eq (T&& a, T&& b) {
    return std::forward<T>(a) == std::forward<T>(b);
}

} // namespace geo
