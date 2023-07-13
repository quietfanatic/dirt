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
    char v;
    constexpr usize length () const { return 1; }
    constexpr usize write (char* p) const {
        *p = v;
        return 1;
    }
};

template <>
struct StringConversion<bool> {
    bool v;
    constexpr usize length () const { return 1; }
    constexpr usize write (char* p) const {
        *p = v ? '1' : '0';
        return 1;
    }
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
    constexpr usize length () const { return len; }
    constexpr usize write (char* p) const {
        expect(len > 0);
         // Awkward incantation to keep the compiler from overkilling the loop
         // with a memcpy
        usize end = len <= max_digits<T> ? len : max_digits<T>;
        for (usize i = 0; i < end; ++i) {
            p[i] = digits[i];
        }
        return len;
    }
};
template <class T> requires (requires (const T& v) { Str(v); })
struct StringConversion<T> {
    Str v;
    constexpr StringConversion (const T& v) : v(Str(v)) { }
    constexpr usize length () const { return v.size(); }
    constexpr usize write (char* p) const {
        for (usize i = 0; i < v.size(); i++) {
            p[i] = v[i];
        }
        return v.size();
    }
};

 // If we don't add this expect(), the compiler emits extra branches for when
 // the total size overflows to 0, but those branches just crash anyway.
constexpr
void cat_add_no_overflow (usize& a, usize b) {
    expect(a + b <= UniqueString::max_size_);
    a += b;
}

template <class... Tail> inline
void cat_append (
    ArrayImplementation<ArrayClass::UniqueS, char>& h, Tail&&... t
) {
    if constexpr (sizeof...(Tail) > 0) {
        usize total_size = h.size;
        (cat_add_no_overflow(total_size, t.length()), ...);
        reinterpret_cast<UniqueString&>(h).reserve_plenty(total_size);
        ((h.size += t.write(h.data + h.size)), ...);
    }
}

} // in

 // Concatenation for character strings.  Returns the result of printing all the
 // arguments, concatenated into a single string.
template <class Head, class... Tail> inline
UniqueString cat (Head&& h, Tail&&... t) {
    if constexpr (
        std::is_same_v<Head&&, UniqueString&&> ||
        std::is_same_v<Head&&, SharedString&&> ||
        std::is_same_v<Head&&, AnyString&&>
    ) {
        if (h.unique()) {
            ArrayImplementation<ArrayClass::UniqueS, char> impl;
            impl.size = h.size(); impl.data = h.mut_data();
            h.unsafe_set_empty();
            in::cat_append(
                impl, in::StringConversion<std::remove_cvref_t<Tail>>(t)...
            );
            return UniqueString::UnsafeConstructOwned(impl.data, impl.size);
        }
    }
    ArrayImplementation<ArrayClass::UniqueS, char> impl = {};
    in::cat_append(
        impl, in::StringConversion<std::remove_cvref_t<Head>>(h),
        in::StringConversion<std::remove_cvref_t<Tail>>(t)...
    );
    return UniqueString::UnsafeConstructOwned(impl.data, impl.size);
}


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
