// Utilities for dealing with floating point numbers.
// You probably don't want to include this directly, include scalar.h instead.

#pragma once

#include <cmath>
#include <bit>
#include <limits>
#include "common.h"
#include "values.h"
#include "type_traits.h"

namespace geo {

///// PROPERTIES

template <Floating T>
constexpr bool defined (T a) {
    return a == a;
}


 // MEGA TODO!  Something seems to have declared finite(double) in the global
 // scope!  What should we do about this?
template <Floating T>
constexpr bool finite (T a) {
     // A full exponent mask means the number is not finite.  This ends up being
     // much quicker than comparing against GNAN, GINF, and -GINF individually,
     // because the optimizer basically doesn't touch floating point
     // expressions.
    auto rep = std::bit_cast<SameSizeInt<T>>(a);
    auto mask = TypeTraits<T>::EXPONENT_MASK;
     // This compiles slightly smaller than (rep & mask) != mask
    return ~rep & mask;
}

 // AKA sqr
template <Floating T>
constexpr T length2 (T v) { return v * v; }
 // Okay, I admit, I just wanted a constexpr abs
template <Floating T>
constexpr T length (T v) {
     // The compiler REALLY does not like optimizing floating point code.
    //return v < 0 ? -v : v;
     // This is a little better but it still loads static data on x64 (also it's
     // not constexpr).
    //return fabs(v);
     // This is still a bit unoptimal due to moving the data between float
     // (vector) and integer registers, but it beats branching and/or reading
     // memory.
    auto rep = std::bit_cast<SameSizeInt<T>>(v);
    return std::bit_cast<T>(rep & ~TypeTraits<T>::SIGN_BIT);
}

///// CONSTEXPR SQUARE ROOT
// Calling these root2 instead of sqrt to avoid ambiguity with std::sqrt

template <Floating T>
constexpr T slow_root2 (T v) {
    if (v == GINF) return GINF;
    else if (!(v >= 0)) return GNAN;
    T curr = v;
    T prev = 0;
    while (curr != prev) {
        prev = curr;
        curr = T(0.5) * (curr + v / curr);
    }
    return curr;
}

constexpr float root2 (float v) {
#if HAS_BUILTIN(__builtin_sqrtf)
    return __builtin_sqrtf(v);
#else
    if consteval { return slow_root2(v); }
    else { return std::sqrtf(v); }
#endif
}

constexpr double root2 (double v) {
#if HAS_BUILTIN(__builtin_sqrt)
    return __builtin_sqrt(v);
#else
    if consteval { return slow_root2(v); }
    else { return std::sqrt(v); }
#endif
}
 // In case you define other floating point types, I guess?
template <Floating T>
constexpr T root2 (T v) { return slow_root2(v); }

///// COMPARISONS

 // Checks that the representations of the two floats are exactly the same.
 // Different NAN values will be unequal.
template <Floating T>
constexpr bool exact_eq (T a, T b) {
    return std::bit_cast<SameSizeInt<T>>(a) == std::bit_cast<SameSizeInt<T>>(b);
}

///// MODIFIERS

 // Round toward 0.  These will debug-assert if the number is NAN or can't fit
 // in an integer of the same size.
template <Floating T>
constexpr SameSizeInt<T> trunc (T a) {
    expect(a >= SameSizeInt<T>(-GINF) && a <= SameSizeInt<T>(GINF));
    return SameSizeInt<T>(a);
}

 // Round towards nearest integer.  0.5 => 1, -0.5 => -1
template <Floating T>
constexpr SameSizeInt<T> round (T a) {
    if (a >= 0) return trunc(a + T(0.5));
    else return trunc(a - T(0.5));
}

 // Round toward negative infinity
template <Floating T>
constexpr SameSizeInt<T> floor (T a) {
    if (a >= 0) return trunc(a);
    else return SameSizeInt<T>(-GINF) - trunc(SameSizeInt<T>(-GINF) - a);
}

 // Round toward positive infinity
template <Floating T>
constexpr SameSizeInt<T> ceil (T a) {
    if (a > 0) return SameSizeInt<T>(GINF) - trunc(SameSizeInt<T>(GINF) - a);
    else return trunc(a);
}

 // Get next larger representable value.
 // guarantees next_quantum(v) > v unless v is NAN or +INF.
template <Floating T>
constexpr T next_quantum (T v) {
    auto rep = std::bit_cast<SameSizeInt<T>>(v);
    if (geo::finite(v)) [[likely]] {
        if (rep < 0) {
            if (
                rep == std::bit_cast<SameSizeInt<T>>(TypeTraits<T>::MINUS_ZERO)
            ) [[unlikely]] {
                 // -0 == 0, so skip over 0
                rep = 1;
            }
            else rep -= 1;
        }
        else rep += 1;
    }
    else if (rep == std::bit_cast<SameSizeInt<T>>(TypeTraits<T>::MINUS_INF)) {
        rep -= 1;
    }
    return std::bit_cast<T>(rep);
}

 // Get next smaller representable value.
 // guarantees prev_quantum(v) < v unless v is NAN or -INF.
template <Floating T>
constexpr T prev_quantum (T v) {
    auto rep = std::bit_cast<SameSizeInt<T>>(v);
    if (geo::finite(v)) [[likely]] {
        if (rep > 0) {
            if (
                rep == std::bit_cast<SameSizeInt<T>>(TypeTraits<T>::PLUS_ZERO)
            ) [[unlikely]] {
                 // 0 == -0, so skip over -0
                rep = -1;
            }
            else rep -= 1;
        }
        else rep += 1;
    }
    else if (rep == std::bit_cast<SameSizeInt<T>>(TypeTraits<T>::PLUS_INF)) {
        rep -= 1;
    }
    return std::bit_cast<T>(rep);
}

 // AKA sign for scalars
template <Floating T>
constexpr T normalize (T v) {
     // (Can't use (v > 0) - (v < 0) because it converts NAN to 0)
     // There might be a better way to do this but I can't find it
    return v > 0 ? 1 : v < 0 ? -1 : v;
}

///// COMBINERS

 // These will not work if a / b is inordinately large.
template <Floating A, Floating B>
constexpr A mod (A a, B b) {
    A ratio = a / b;
    if (ratio >= SameSizeInt<A>(-GINF) && ratio <= SameSizeInt<A>(GINF)) {
        return a - floor(ratio) * b;
    }
    else [[unlikely]] return GNAN;
}
 // Like mod but can return negative
template <Floating A, Floating B>
constexpr A rem (A a, B b) {
    A ratio = a / b;
    if (ratio >= SameSizeInt<A>(-GINF) && ratio <= SameSizeInt<A>(GINF)) {
        return a - trunc(ratio) * b;
    }
    else [[unlikely]] return GNAN;
}

 // AKA copysign, but propagates NANs and returns 0 if b == 0
template <Floating A, Floating B>
constexpr A align (A a, B b) {
    auto rep = std::bit_cast<SameSizeInt<A>>(a);
    if (b > 0) {
        return std::bit_cast<A>(rep & ~TypeTraits<A>::SIGN_BIT);
    }
    else if (b < 0) {
        return std::bit_cast<A>(rep & TypeTraits<A>::SIGN_BIT);
    }
    else return b;
}

 // This is the standard lerping formula.
template <Floating A, Floating B, Fractional T>
constexpr auto lerp (A a, B b, T t) {
    return (1-t)*a + t*b;
}

} // namespace geo
