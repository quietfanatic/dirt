#pragma once

#include <charconv>
#include "arrays.h"
#include "text.h"

namespace uni {
inline namespace strings {

 // Concatenation for character strings.  Returns the result of converting all
 // the arguments to strings, concatenated into a single string.
 //
 // Example: int n; cat("There are ", n, " trees.");
 //
 //   - All ints, float, and double will be converted with std::to_chars.
 //   - unsigned char and signed char are considered integers.
 //   - char by itself is treated like a string of one char.
 //   - bool is written as 0 or 1.
 //   - Any type with .size() and .data() is assumed to be string-like,
 //     and .data() must coerce to const char*.
 //   - Anything that coerces to Str will be coerced to Str.
 //   - Other types can be processed by specializing StringConversion below.
 //
 // For multiple segments, this will be much more efficient than using a binary
 // concatenation operator, because it only does one allocation.
template <class Head, class... Tail>
UniqueString cat (Head&& h, const Tail&... t);
UniqueString cat ();

 // In-place-modifying version of cat.  Named with the English prefix "en"
 // meaning "to" or "onto" or "unto".
template <class Head, class... Tail> inline
Head& encat (Head& h, const Tail&... t) {
    return h = cat(move(h), t...);
}

 // Trait for string conversions.  This must have:
 //    using Self = Type;
 // where Type has either these methods:
 //    usize size () const;
 //    const char* data () const;
 // or these methods:
 //    usize size () const;
 //    char* write (char* out) const;  // returns out + amount written
 // If using the write() method, it's okay for size() to return a little more
 // than write() actually writes.
 //
 // It's acceptable and normal to define Self = StringConversion<Type> and
 // define .size() and .data() on StringConversion<Type> itself.
template <class T>
struct StringConversion;

// Caterator (concatenating iterator) is a special object to pass to cat() which
// uses a function to generate strings (or anything that can be converted to
// strings) and joins them with the given separator.  The passed-in function
// will be called TWICE for every number from 0 to n-1, the first time with
// size() called on each result, and the second time with data() called on each
// result.
//
// If the function is simple, Caterator is likely to be more efficient than
// calling cat() multiple times, because each call to cat() might have to
// reallocate the output string.  If the function is simple and returns a Str,
// the optimizer can probably split the calculation of the Str into its size and
// its data, and calculate only the size in the first loop and the data in the
// second loop.
// If the function is complicated, and it would be slow to loop over it twice,
// you can try caching the results in an array instead, and then calling
// Caterator on the array.
template <class F>
struct Caterator {
    Str separator;
    usize n;
    const F& f;

    constexpr Caterator (Str s, usize n, const F& f) :
        separator(s), n(n), f(f)
    { }

    constexpr usize size () const;
    constexpr char* write (char* out) const;
};

 // Literal suffix for StaticString.  This is usually unnecessary, as raw
 // const char[] arrays are generally treated as static strings by the arrays
 // library.
consteval StaticString operator""_s (const char* p, usize s) {
    return StaticString(p, s);
}

///// DEFAULT STRING CONVERSIONS

namespace in {
template <class T> constexpr u32 max_digits;
template <> constexpr u32 max_digits<u8> = 3;
template <> constexpr u32 max_digits<i8> = 4;
template <> constexpr u32 max_digits<u16> = 5;
template <> constexpr u32 max_digits<i16> = 6;
template <> constexpr u32 max_digits<u32> = 10;
template <> constexpr u32 max_digits<i32> = 11;
template <> constexpr u32 max_digits<u64> = 20;
template <> constexpr u32 max_digits<i64> = 20;
template <> constexpr u32 max_digits<float> = 16;
template <> constexpr u32 max_digits<double> = 24;
template <> constexpr u32 max_digits<long double> = 48; // dunno, seems safe
} // in

template <>
struct StringConversion<char> {
    using Self = StringConversion<char>;
    char c;
    ALWAYS_INLINE constexpr usize size () const { return 1; }
    ALWAYS_INLINE constexpr const char* data () const { return &c; }
};

template <>
struct StringConversion<bool> {
    using Self = StringConversion<bool>;
    char c;
    ALWAYS_INLINE constexpr StringConversion (bool v) : c(v + '0') { }
    ALWAYS_INLINE constexpr usize size () const { return 1; }
    ALWAYS_INLINE constexpr const char* data () const { return &c; }
};

template <class T> requires (std::is_integral_v<T>)
struct StringConversion<T> {
    using Self = StringConversion<T>;
    T v;
    u8 count;
    StringConversion (T v) : v(v) { }
     // Get a close upper bound for the number of digits.
    ALWAYS_INLINE constexpr usize size () {
        bool sign = v < 0;
        count = count_decimal_digits(sign ? -v : v);
        expect(count > 0);
        return count + sign;
    }
    constexpr char* write (char* out) const {
        bool sign = v < 0;
        if (sign) *out++ = '-';
        return write_decimal_digits(out, count, sign ? -v : v);
    }
};

 // This does an extra copy of the characters, which may not be optimal, but
 // it's better than doing the entire conversion twice, and estimating the
 // length ahead of time is much harder for floating point numbers.
template <class T> requires (std::is_floating_point_v<T>)
struct StringConversion<T> {
    using Self = StringConversion<T>;
    char digits [in::max_digits<T>];
    u32 len;
    constexpr StringConversion (T v) {
        if (v != v) {
            std::memcpy(digits, "+nan", len = 4);
        }
        else if (v == std::numeric_limits<T>::infinity()) {
            std::memcpy(digits, "+inf", len = 4);
        }
        else if (v == -std::numeric_limits<T>::infinity()) {
            std::memcpy(digits, "-inf", len = 4);
        }
        else {
            auto [ptr, ec] = std::to_chars(digits, digits + in::max_digits<T>, v);
            expect(ec == std::errc());
            len = ptr - digits;
        }
    }
    ALWAYS_INLINE constexpr usize size () const { return len; }
    ALWAYS_INLINE constexpr const char* data () const { return digits; }
};

template <class T> requires (!std::is_same_v<std::remove_cvref_t<T>, char>)
struct StringConversion<T*> {
    using Self = StringConversion<T*>;
    T* v;
    ALWAYS_INLINE constexpr usize size () const { return sizeof(usize) * 2; }
    constexpr char* write (char* out) const {
        for (usize i = 0; i < size(); i++) {
            u8 nyb = (usize(v) >> (size() - i - 1) * 4) & 0xf;
            *out++ = nyb >= 10 ? 'a' + nyb - 10 : '0' + nyb;
        }
        return out;
    }
};

template <class T> requires (
    requires (const T& v) { v.size(); v.data(); }
)
struct StringConversion<T> {
    using Self = const T&;
};

template <class T> requires (
    requires (const T& v) { Str(v); } &&
    !requires (const T& v) { v.size(); v.data(); }
)
struct StringConversion<T> {
    using Self = Str;
};

///// CAT IMPLEMENTATION

namespace in {

ALWAYS_INLINE constexpr void cat_add_no_overflow (usize& a, usize b) {
    expect(a + b >= a);
    expect(a + b >= b);
    a += b;
}

 // Write conversion objects with .data()
template <class T> requires (
    requires (T t, usize s, const char* p) { s = t.size(); p = t.data(); }
)
ALWAYS_INLINE char* cat_write (char* out, const T& t) {
    usize s = t.size();
    const char* p = t.data();
     // Apparently it's illegal to pass null to memcpy even if the size is 0.  This
     // is irritating because every known implementation of memcpy will just no-op
     // for 0 size like you'd expect, but the standards say Undefined Behavior.
    return s ? s + (char*)std::memcpy(out, p, s) : out;
}

 // Capitulate to things returning char8_t*
template <class T> requires (
    requires (T t, usize s, const char8_t* p) { s = t.size(); p = t.data(); }
)
ALWAYS_INLINE char* cat_write (char* out, const T& t) {
    usize s = t.size();
    const char8_t* p = t.data();
    return s ? s + (char*)std::memcpy(out, p, s) : out;
}

 // Write conversion objects with .write()
template <class T> requires (
    requires (T t, usize s, char* out) { s = t.size(); out = t.write(out); }
)
ALWAYS_INLINE char* cat_write (char* out, const T& t) {
    return t.write(out);
}

template <class... Tail> ALWAYS_INLINE
void cat_append (UniqueString& h, Tail&&... t) {
    if constexpr (sizeof...(Tail) > 0) {
        usize cap = h.size();
        (cat_add_no_overflow(cap, t.size()), ...);
        h.reserve_plenty(cap);
        char* out = h.end();
        ((
            out = cat_write(out, t)
        ), ...);
        h.impl.size = out - h.impl.data;
    }
}

template <class Head, class... Tail> ALWAYS_INLINE
UniqueString cat_construct (Head&& h, Tail&&... t) {
     // Record of investigations: I was poking around in the disassembly on an
     // optimized build, and found that this function was producing a call to
     // UniqueString::remove_ref, which indicated that a move construct (and
     // destruct) was happening, instead of the NVRO I expected.  After some
     // investigation, it turned out that wrapping the entire function in an
     // if constexpr was screwing with GCC's NVRO but only when LTO was enabled.
     // (Also, I was misunderstanding the variadic folding syntax and was using
     // if constexpr to compensate, so it is no longer necessary).
    usize cap = h.size();
    (cat_add_no_overflow(cap, t.size()), ...);
    auto r = UniqueString(Capacity(cap));
    char* out = cat_write(r.impl.data, h);
    ((
        out = cat_write(out, t)
    ), ...);
    r.impl.size = out - r.impl.data;
    return r;
}

} // in

template <class Head, class... Tail> inline
UniqueString cat (Head&& h, const Tail&... t) {
    if constexpr (std::is_same_v<Head&&, UniqueString&&>) {
        in::cat_append(h,
            typename StringConversion<std::remove_cvref_t<Tail>>::Self(t)...
        );
        return move(h);
    }
    else return in::cat_construct(
        typename StringConversion<std::remove_cvref_t<Head>>::Self(
            std::forward<Head>(h)
        ),
        typename StringConversion<std::remove_cvref_t<Tail>>::Self(t)...
    );
}
inline UniqueString cat () { return ""; }

///// CATERATOR IMPLEMENTATION

template <class F>
ALWAYS_INLINE constexpr usize Caterator<F>::size () const {
    usize r = separator.size() * (n ? n-1 : 0);
    for (usize i = 0; i < n; i++) {
        r += typename StringConversion<decltype(f(i))>::Self(f(i)).size();
    }
    return r;
}
template <class F>
ALWAYS_INLINE constexpr char* Caterator<F>::write (char* out) const {
    if (n) {
        usize i = 0;
        out = in::cat_write(out,
            typename StringConversion<decltype(f(i))>::Self(f(i))
        );
        for (i = 1; i < n; i++) {
            out = in::cat_write(out, separator);
            out = in::cat_write(out,
                typename StringConversion<decltype(f(i))>::Self(f(i))
            );
        }
    }
    return out;
}

template <class F>
struct StringConversion<Caterator<F>> {
    using Self = Caterator<F>;
};

} // strings
} // uni
