// Some convenient text-related functions

#pragma once

#include "common.h"
#include "arrays.h"

namespace uni {

 // Does a comparison for a "natural sort", where numbers within the string are
 // sorted by their numeric value regardless of how many digits they are.  The
 // behavior of this is not strictly specified and may change in future updates,
 // but this is the current behavior:
 //
 // - Characters are ordered as follows: NUL, /, \, ., then everything else
 //   according to byte order (which, for UTF-8 strings, is unicode order).
 //   This is to order filepaths in an intuitive manner, without filename
 //   extensions and directory contents interfering.
 // - Runs of ascii digits are sorted numerically.  If they evaluate to the same
 //   number, then the longer run (the one with more leading 0s) is sorted
 //   after).
 // - Other types of unicode digits are not treated specially.
 // - When comparing ascii digits to non-digits, the digits are just treated as
 //   normal ascii characters.
 //
int natural_compare (Str a, Str b) noexcept;
 // For use with STL std::sort.  You can also use it with std::stable_sort but
 // that will call the function twice as often as necessary.
inline bool natural_lessthan (Str a, Str b) {
    return natural_compare(a, b) < 0;
}

 // Returns -1 if the given char is not [0-9a-fA-F]
constexpr int from_hex_digit (char c) {
    if (c >= '0' && c <= '9') return c - '0';
    else {
        c &= ~('a' & ~'A'); // Clear lowercase bit
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        else return -1;
    }
}

 // Returns NUL if the given int is not 0..15
constexpr char to_hex_digit (u8 digit) {
    if (digit >= 16) [[unlikely]] return 0;
    return digit + (digit < 10 ? '0' : 'A' - 10);
}

inline bool ascii_is_lower (Str s) {
    for (auto c : s) if (c >= 'A' && c <= 'Z') return false;
    return true;
}

inline UniqueString ascii_to_upper (Str s) {
    return UniqueString(s.size(), [s](usize i){
        char c = s[i];
        if (c >= 'a' && c <= 'z') c &= ~('a' & ~'A');
        return c;
    });
}

inline UniqueString ascii_to_lower (Str s) {
    return UniqueString(s.size(), [s](usize i){
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c |= ('a' & ~'A');
        return c;
    });
}

inline bool ascii_eqi (Str a, Str b) {
    if (a.size() != b.size()) return false;
    for (u32 i = 0; i < a.size(); i++) {
        char ac = a[i];
        char bc = b[i];
        if (ac != bc) {
            if ((ac ^ bc) == 0x20) {
                 // If they differ by only the case bit, we only need to check that
                 // one of them is alphabetical.
                ac |= 0x20;
                if (ac >= 'a' && ac <= 'z') { }
                else return false;
            }
            else return false;
        }
    }
    return true;
}

 // Like std::from_chars but smaller.  Returns {start, 0} if the number
 // overflows or has no digits.  There are no other error conditions.  Does not
 // accept an initial + or -.
template <class T>
struct ReadResult {
    const char* p;
    T value;
};
template <class T>
ReadResult<T> read_decimal_digits (
    const char* start, const char* end
) noexcept {
    ReadResult<T> r {start, 0};
    while (r.p != end) {
        u8 digit = *r.p - '0';
        if (digit > 9) return r;
        auto old = r.value;
        r.value = r.value * 10 + digit;
        if (r.value < old) [[unlikely]] return {start, 0};
        r.p++;
    }
    return r;
}

// Like read_decimal_digits but hexadecimal.  Case insensitive.  Does not accept
// an initial 0x or 0X.
template <class T>
ReadResult<T> read_hex_digits (
    const char* start, const char* end
) noexcept {
    ReadResult<T> r {start, 0};
    while (r.p != end) {
        int digit = from_hex_digit(*r.p);
        if (digit < 0) return r;
        auto old = r.value;
        r.value = (r.value << 4) + digit;
        if (r.value < old) [[unlikely]] return {start, 0};
        r.p++;
    }
    return r;
}

 // Returns the number of decimal digits in the unsigned number.  Can return 1
 // through 20.  You can also think of this as 1+floor(log10(v)) except it
 // returns 1 for 0 instead of -inf.
[[gnu::const]]
u32 count_decimal_digits (u64 v) noexcept;

 // Writes out the decimal form of v.  Count must be the number returned by
 // count_decimal_digits(v).  Returns p + count (the end of the written number).
char* write_decimal_digits (char* p, u32 count, u64 v) noexcept;

constexpr
u32 count_hex_digits (u64 v) {
    if (v <= 0xf) return 1;
    return 16 - (std::countl_zero(v) >> 2);
}

constexpr
char* write_hex_digits (char* p, u32 count, u64 v) {
    expect(count >= 1);
    if (count == 1) {
        *p++ = (v < 10 ? '0' : 'a' - 10) + v;
        return p;
    }
    char* b = p;
    char* e = p + count;
    for (p = e; p > b; p--) {
        char nyb = v & 0xf;
        p[-1] = (nyb < 10 ? '0' : 'a' - 10) + nyb;
        v >>= 4;
    }
    return e;
}

} // namespace uni
