#pragma once

#include <charconv>
#include "arrays.h"

namespace uni {
inline namespace strings {

 // Literal suffix for StaticString.  This is usually unnecessary.
consteval StaticString operator""_s (const char* p, usize s) {
    return StaticString(p, s);
}

namespace in {

template <class T>
struct StringConversion;

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

template <class T> constexpr uint32 max_digits;
template <> constexpr uint32 max_digits<uint8> = 3;
template <> constexpr uint32 max_digits<int8> = 4;
template <> constexpr uint32 max_digits<uint16> = 5;
template <> constexpr uint32 max_digits<int16> = 6;
template <> constexpr uint32 max_digits<uint32> = 10;
template <> constexpr uint32 max_digits<int32> = 11;
template <> constexpr uint32 max_digits<uint64> = 19;
template <> constexpr uint32 max_digits<int64> = 20;
template <> constexpr uint32 max_digits<float> = 16;
template <> constexpr uint32 max_digits<double> = 24;
template <> constexpr uint32 max_digits<long double> = 48; // dunno, seems safe

template <class T> requires (std::is_arithmetic_v<T>)
struct StringConversion<T> {
    using Self = StringConversion<T>;
    char digits [max_digits<T>];
    uint32 len;
    constexpr StringConversion (T v) {
        if constexpr (std::is_floating_point_v<T>) {
            if (v != v) {
                std::memcpy(digits, "+nan", len = 4);
                return;
            }
            else if (v == std::numeric_limits<T>::infinity()) {
                std::memcpy(digits, "+inf", len = 4);
                return;
            }
            else if (v == -std::numeric_limits<T>::infinity()) {
                std::memcpy(digits, "-inf", len = 4);
                return;
            }
        }
        auto [ptr, ec] = std::to_chars(digits, digits + max_digits<T>, v);
        expect(ec == std::errc());
        len = ptr - digits;
    }
    ALWAYS_INLINE constexpr usize size () const { return len; }
    ALWAYS_INLINE constexpr const char* data () const { return digits; }
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

ALWAYS_INLINE constexpr void cat_add_no_overflow (usize& a, usize b) {
    expect(a + b >= a);
    expect(a + b >= b);
    a += b;
}

template <class... Tail> inline
void cat_append (UniqueString& h, const Tail&... t) {
    if constexpr (sizeof...(Tail) > 0) {
        usize cap = h.size();
        (cat_add_no_overflow(cap, t.size()), ...);
        h.reserve_plenty(cap);
        char* pos = h.end();
        ((
            pos = t.size() + (char*)std::memcpy(pos, t.data(), t.size())
        ), ...);
        h.unsafe_set_size(pos - h.begin());
    }
}

template <class Head, class... Tail> inline
UniqueString cat_construct (Head&& h, const Tail&... t) {
     // Record of investigations: I was poking around in the disassembly on an
     // optimized build, and found that this function was producing a call to
     // UniqueString::remove_ref, which indicated that a move construct (and
     // destruct) was happening, instead of the NVRO I expected.  After some
     // investigation, it turned out that wrapping the entire function in an
     // if constexpr was screwing with GCC's NVRO only when LTO was enabled.
     // (Also, I was misunderstanding this variadic folding syntax and was using
     // if constexpr to compensate, so it is no longer necessary).
    usize cap = h.size();
    (cat_add_no_overflow(cap, t.size()), ...);
    auto r = UniqueString(Capacity(cap));
    char* pos = h.size() + (char*)std::memcpy(r.data(), h.data(), h.size());
    ((
        pos = t.size() + (char*)std::memcpy(pos, t.data(), t.size())
    ), ...);
    r.unsafe_set_size(pos - r.begin());
    return r;
}

} // in

 // Concatenation for character strings.  Returns the result of printing all the
 // arguments, concatenated into a single string.
template <class Head, class... Tail> inline
UniqueString cat (Head&& h, const Tail&... t) {
    if constexpr (std::is_same_v<Head&&, UniqueString&&>) {
        in::cat_append(h,
            typename in::StringConversion<std::remove_cvref_t<Tail>>::Self(t)...
        );
        return move(h);
    }
    else return in::cat_construct(
        typename in::StringConversion<std::remove_cvref_t<Head>>::Self(
            std::forward<Head>(h)
        ),
        typename in::StringConversion<std::remove_cvref_t<Tail>>::Self(t)...
    );
}
inline UniqueString cat () { return ""; }

} // strings
} // uni

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"
namespace tap {
template <uni::ArrayClass ac>
struct Show<uni::ArrayInterface<ac, char>> {
    std::string show (const uni::ArrayInterface<ac, char>& v) {
        return std::string(uni::cat("\"", v, "\""));
    }
};
}
#endif
