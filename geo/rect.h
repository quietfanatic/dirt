 // 2D rectangles.  They store the four sides in the order of l, b, t, r.
 // Unlike many rectangle types, they don't store width and height; those are
 // calculated.  These rectangles assume a y-up coordinate system.  You can use
 // them in a y-down coordinate system, but the names "b" and "t" will be
 // backwards.

#pragma once

#include "../ayu/reflection/describe.h"
#include "common.h"
#include "range.h"
#include "vec.h"

namespace geo {
using namespace uni;

template <class T>
struct GRect;

using Rect = GRect<float>;
using DRect = GRect<double>;
using IRect = GRect<i32>;
using LRect = GRect<i64>;
 // I can imagine use cases for this.
using BRect = GRect<bool>;

 // Like ranges, Rects are considered to include the left and bottom, and
 // exclude the right and top.
template <class T>
struct GRect {
    T l;
    T b;
    T r;
    T t;

    constexpr GRect () : l(0), b(0), r(0), t(0) { }
    constexpr GRect (T l, T b, T r, T t) : l(l), b(b), r(r), t(t) {
     // You are not allowed to create a rectangle with some sides defined but
     // not others.
#ifndef NDEBUG
        bool any_defined = l == l || b == b || r == r || t == t;
        bool all_defined = l == l && b == b && r == r && t == t;
        expect(any_defined == all_defined);
#endif
    }
     // Create the undefined rectangle.  Most operations are not defined on the
     // undefined rectangle.
    constexpr GRect (GNAN_t n) : l(n), b(n), r(n), t(n) { }
     // Create an infinitely large (possibly negative) rectangle.
    constexpr GRect (GINF_t i) : l(-i), b(-i), r(i), t(i) { }
     // Create from lower-left and upper-right corners
    constexpr GRect (const GVec<T, 2>& lb, const GVec<T, 2>& rt) :
        l(lb.x), b(lb.y), r(rt.x), t(rt.y)
    { }
     // Create from two 1-dimensional ranges.
    constexpr GRect (const GRange<T>& lr, const GRange<T>& bt) :
        l(lr.l), b(bt.l), r(lr.r), t(bt.r)
    { }

     // Convert from another rect type
    template <class T2> constexpr explicit
    GRect (const GRect<T2>& o) : l(o.l), b(o.b), r(o.r), t(o.t) { }

     // Don't use this to check for definedness or area = 0.  It only checks
     // that each side is strictly zero.
    constexpr explicit operator bool () const { return l || b || r || t; }
};

template <class T>
struct TypeTraits<GRect<T>> {
    using Widened = GRect<Widen<T>>;
    static constexpr bool integral = false;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = TypeTraits<T>::is_signed;
};

///// PROPERTIES

 // Get a corner of the rectangle
template <class T>
constexpr GVec<T, 2> lb (const GRect<T>& a) { return {a.l, a.b}; }
template <class T>
constexpr GVec<T, 2> rb (const GRect<T>& a) { return {a.r, a.b}; }
template <class T>
constexpr GVec<T, 2> rt (const GRect<T>& a) { return {a.r, a.t}; }
template <class T>
constexpr GVec<T, 2> lt (const GRect<T>& a) { return {a.l, a.t}; }

 // Get center point
template <class T>
constexpr GVec<T, 2> center (const GRect<T>& a) {
    return {center(lr(a)), center(bt(a))};
}

 // Get one dimension of the rectangle
template <class T>
constexpr GRange<T> lr (const GRect<T>& a) { return {a.l, a.r}; }
template <class T>
constexpr GRange<T> bt (const GRect<T>& a) { return {a.b, a.t}; }

 // Two-dimensional size
template <class T>
constexpr GVec<T, 2> size (const GRect<T>& a) { return {a.r - a.l, a.t - a.b}; }
 // width(a) == size(a).x == size(lr(a))
template <class T>
constexpr T width (const GRect<T>& a) { return a.r - a.l; }
 // height(a) == size(a).y == size(bt(a))
template <class T>
constexpr T height (const GRect<T>& a) { return a.t - a.b; }

 // Will debug assert if some but not all elements are undefined
template <class T>
constexpr bool defined (const GRect<T>& a) {
#ifndef NDEBUG
    bool any_defined = a.l == a.l || a.b == a.b || a.r == a.r || a.t == a.t;
    bool all_defined = a.l == a.l && a.b == a.b && a.r == a.r && a.t == a.t;
    expect(any_defined == all_defined);
#endif
    return defined(a.l);
}

 // Returns false if any side is NAN, INF, or -INF
template <class T>
constexpr bool finite (const GRect<T>& a) {
    return finite(a.l) && finite(a.b) && finite(a.r) && finite(a.t);
}

 // Will be negative if one of width() or height() is negative (but not both)
template <class T>
constexpr auto area (const GRect<T>& a) {
    return widen(a.r - a.l) * widen(a.t - a.b);
}

 // Area is 0 (either width or height is 0)
template <class T>
constexpr bool empty (const GRect<T>& a) {
    return a.l == a.r || a.b == a.t;
}

 // Both width and height are non-negative.  (proper(NAN) == true)
template <class T>
constexpr bool proper (const GRect<T>& a) {
    return proper(lr(a)) && proper(bt(a));
}

 // The bounding box of a rectangle is itself
template <class T>
constexpr const GRect<T>& bounds (const GRect<T>& a) { return a; }

///// MODIFIERS

 // Change inclusivity of sides
template <class T>
constexpr GRect<T> exclude_lb (const GRect<T>& a) {
    return {exclude_l(lr(a)), exclude_l(bt(a))};
}
template <class T>
constexpr GRect<T> include_rt (const GRect<T>& a) {
    return {include_r(lr(a)), include_r(bt(a))};
}

 // Flip both horizontally and vertically but keep the center in the same place.
 // To flip around the origin, multiply by -1 instead.  Because both dimensions
 // are flipped, this won't change the sign of the area.
template <class T>
constexpr GRect<T> invert (const GRect<T>& a) {
    return {a.r, a.t, a.l, a.b};
}

 // Flip horizontally
template <class T>
constexpr GRect<T> invert_h (const GRect<T>& a) {
    return {a.r, a.b, a.l, a.t};
}

 // Flip vertically
template <class T>
constexpr GRect<T> invert_v (const GRect<T>& a) {
    return {a.l, a.t, a.r, a.b};
}

 // If not proper, flip horizontally and/or vertically to make it proper.
template <class T>
constexpr GRect<T> properize (const GRect<T>& a) {
    return {properize(a.lr()), properize(a.bt())};
}

 // Arithmetic operators
#define GRECT_UNARY_OP(op) \
template <class T> \
constexpr GRect<T> operator op (const GRect<T>& a) { \
    return GRect<T>(op a.l, op a.b, op a.t, op a.r); \
}
GRECT_UNARY_OP(+)
GRECT_UNARY_OP(-)
#undef GRECT_UNARY_OP

 // Rounding each component
#define GRECT_ROUND_OP(op) \
template <class T> \
constexpr auto op (const GRect<T>& a) { \
    return GRect<decltype(op(a.l))>(op(a.l), op(a.b), op(a.r), op(a.t)); \
}
GRECT_ROUND_OP(trunc)
GRECT_ROUND_OP(round)
GRECT_ROUND_OP(floor)
GRECT_ROUND_OP(ceil)
#undef GRECT_ROUND_OP

///// RELATIONSHIPS

template <class T>
constexpr bool operator == (const GRect<T>& a, const GRect<T>& b) {
    return a.l == b.l && a.b == b.b && a.r == b.r && a.t == b.t;
}
template <class T>
constexpr bool operator != (const GRect<T>& a, const GRect<T>& b) {
    return a.l != b.l || a.b != b.b || a.r != b.r || a.t != b.t;
}

// These assume the rectangles are proper, and may give unintuitive results
// if they aren't.

 // a and b are overlapping.  Returns false if the rectangles are only touching
 // on the border.
 // overlaps(a, b) == !empty(a & b)
template <class T>
constexpr bool overlaps (const GRect<T>& a, const GRect<T>& b) {
    return overlaps(lr(a), lr(b)) && overlaps(bt(a), bt(b));
}
 // touches(a, b) == proper(a & b)
template <class T>
constexpr bool touches (const GRect<T>& a, const GRect<T>& b) {
    return touches(lr(a), lr(b)) && touches(bt(a), bt(b));
}

 // b is fully contained in a.
 // contains(a, b) == ((a | b) == a) == ((a & b) == b)
template <class T>
constexpr bool contains (const GRect<T>& a, const GRect<T>& b) {
    return contains(lr(a), lr(b)) && contains(bt(a), bt(b));
}
 // b is contained in a.  Note that the left and bottom are inclusive but the
 // right and top are exclusive.
template <class T>
constexpr bool contains (const GRect<T>& a, const GVec<T, 2>& b) {
    return contains(lr(a), b.x) && contains(bt(a), b.y);
}

///// COMBINERS

#define GRECT_GVEC_OP(op) \
template <class T> \
constexpr GRect<T> operator op (const GRect<T>& a, const GVec<T, 2>& b) { \
    return {a.l op b.x, a.b op b.y, a.r op b.x, a.t op b.y}; \
} \
template <class T> \
constexpr GRect<T> operator op (const GVec<T, 2>& a, const GRect<T>& b) { \
    return {a.x op b.l, a.y op b.b, a.x op b.r, a.y op b.t}; \
}
GRECT_GVEC_OP(+)
GRECT_GVEC_OP(-)
GRECT_GVEC_OP(*)
GRECT_GVEC_OP(/)
#undef GRECT_GVEC_OP

#define GRECT_GVEC_OPEQ(op) \
template <class T> \
constexpr GRect<T>& operator op (GRect<T>& a, const GVec<T, 2>& b) { \
    a.l op b.x; a.b op b.y; a.r op b.x; a.t op b.y; \
    return a; \
}
GRECT_GVEC_OPEQ(+=)
GRECT_GVEC_OPEQ(-=)
GRECT_GVEC_OPEQ(*=)
GRECT_GVEC_OPEQ(/=)
#undef GRECT_GVEC_OPEQ

#define GRECT_SCALAR_OP(op) \
template <class T> \
constexpr GRect<T> operator op (const GRect<T>& a, T b) { \
    return {a.l op b, a.b op b, a.r op b, a.t op b}; \
} \
template <class T> \
constexpr GRect<T> operator op (T a, const GRect<T>& b) { \
    return {a op b.l, a op b.b, a op b.r, a op b.t}; \
}
GRECT_SCALAR_OP(*)
GRECT_SCALAR_OP(/)
#undef GRECT_SCALAR_OP

#define GRECT_SCALAR_OPEQ(op) \
template <class T> \
constexpr GRect<T>& operator op (GRect<T>& a, T b) { \
    a.l op b; a.b op b; a.r op b; a.t op b; \
    return a; \
}
GRECT_SCALAR_OPEQ(*=)
GRECT_SCALAR_OPEQ(/=)
#undef GRECT_SCALAR_OPEQ

 // Box union.  May give unintuitive results if a and b aren't both proper.
template <class T>
constexpr GRect<T> operator | (const GRect<T>& a, const GRect<T>& b) {
    return {min(a.l, b.l), min(a.b, b.b), max(a.r, b.r), max(a.t, b.t)};
}
template <class T>
constexpr GRect<T>& operator |= (GRect<T>& a, const GRect<T>& b) {
    return a = a | b;
}
 // Box intersection.  If a and b aren't intersecting, the result is not proper.
template <class T>
constexpr GRect<T> operator & (const GRect<T>& a, const GRect<T>& b) {
    return {max(a.l, b.l), max(a.b, b.b), min(a.r, b.r), min(a.t, b.t)};
}
template <class T>
constexpr GRect<T>& operator &= (GRect<T>& a, const GRect<T>& b) {
    return a = a & b;
}

template <class A, class B, Fractional T>
constexpr auto lerp (
    const GRect<A>& a,
    const GRect<B>& b,
    T t
) {
    return GRect<decltype(lerp(a.l, b.l, t))>{
        lerp(a.l, b.l, t),
        lerp(a.b, b.b, t),
        lerp(a.r, b.r, t),
        lerp(a.t, b.t, t)
    };
}

 // If p is outside of a, returns the closest point to p contained in a.
template <class T>
constexpr GVec<T, 2> clamp (const GVec<T, 2>& p, const GRect<T>& a) {
    return {clamp(p.x, a.lr()), clamp(p.y, a.bt())};
}

} // namespace geo

///// GENERIC AYU DESCRIPTION

AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(geo::GRect<T>),
    []{
        if constexpr (std::is_same_v<T, float>) return desc::name("geo::Rect");
        else if constexpr (std::is_same_v<T, double>) return desc::name("geo::DRect");
        else if constexpr (std::is_same_v<T, uni::i32>) return desc::name("geo::IRect");
        else if constexpr (std::is_same_v<T, uni::i64>) return desc::name("geo::LRect");
        else if constexpr (std::is_same_v<T, bool>) return desc::name("geo::BRect");
        else return desc::computed_name([]()->uni::AnyString{
            return uni::cat(
                "geo::GRect<", ayu::Type::For<T>().name(), '>'
            );
        });
    }(),
    desc::elems(
        desc::elem(&geo::GRect<T>::l),
        desc::elem(&geo::GRect<T>::b),
        desc::elem(&geo::GRect<T>::r),
        desc::elem(&geo::GRect<T>::t)
    )
)
