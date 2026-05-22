// Functions involving integral types
// You probably don't want to include this directly, include scalar.h instead.

#pragma once

#include "common.h"
#include "type_traits.h"

namespace geo {

///// PROPERTIES

 // AKA sqr
template <SignedIntegral T>
constexpr MakeUnsigned<Widen<T>> length2 (T v) {
    return widen(v) * widen(v);
}

 // AKA abs
template <SignedIntegral T>
constexpr MakeUnsigned<T> length (T v) {
     // Do we need to special-case when v is the smallest negative integer,
     // which when negated equals itself?  No!  Turns out casting that number to
     // unsigned gives the right answer anyway.
    return v >= 0 ? v : -v;
}

///// MODIFIERS

 // AKA sign
template <SignedIntegral T>
constexpr T normalize (T v) {
    return v > 0 ? 1 : v < 0 ? -1 : v;
}

 // These aren't very interesting for integers.
template <class T> requires (Integral<T> || Pointing<T>)
constexpr T next_quantum (T v) { return v+1; }

template <class T> requires (Integral<T> || Pointing<T>)
constexpr T prev_quantum (T v) { return v-1; }

///// COMBINERS

 // mod is like rem, but always has the type and sign of the right side, making
 // a more regular graph.  This might be more intuitive in many cases, and also
 // guarantees that the result is a valid index for an array of size b.  This
 // might not behave properly for extremely large numbers.
template <Integral A, Integral B>
constexpr B mod (A a, B b) {
    if constexpr (UnsignedIntegral<B>) {
         // Can't do -a % -b because unary - on u32 returns u32 lolll
        A r = a % b;
        return r < 0 ? B(r) + b : B(r);
    }
    else if (a >= 0) return a % b;
    else return -a % -b;
}
 // The % operator returns a result with the sign of the left side, resulting
 // in a graph that looks like this around (0,0) for positive b.
 //            /| /| /
 //           / |/ |/
 //    /| /| /
 //   / |/ |/
template <Integral A, Integral B>
constexpr A rem (A a, B b) { return a % b; }

 // AKA copysign
template <SignedIntegral A, SignedIntegral B>
constexpr A align (A a, B b) {
    return b >= 0 ? length(a) : -length(a);
}

 // This algorithm is slightly better for integers than a(1-t) * bt
template <Integral A, Integral B, Fractional T>
constexpr std::common_type<A, B> lerp (A a, B b, T t) {
    return a + round((b - a) * t);
}

 // Lerping pointers!  This is stupid and I'm sure I'll regret it.
template <Pointing P, Fractional T>
constexpr P lerp (P a, P b, T t) {
     // For safety, ensure we don't go outside the given range.  Note that if we
     // are given a standard begin-end pair (where end cannot be dereferenced),
     // the undereferencable end will be returned if t == 1.
    assume(t >= T(0) && t <= T(1));
    return a + round((b - a) * t);
}

} // namespace geo
