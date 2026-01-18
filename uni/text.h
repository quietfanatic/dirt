// Some convenient text-related functions

#pragma once

#include "common.h"
#include "arrays.h"

namespace uni {

 // Does a comparison for a "natural sort", where numbers within the string are
 // sorted by their numeric value regardless of how many digits they are.  The
 // behavior of corner cases may change in future updates.
int natural_compare (Str a, Str b) noexcept;
inline bool natural_lessthan (Str a, Str b) {
    return natural_compare(a, b) < 0;
}

 // Like natural_compare, but for file paths.  The technical difference is that
 // '/' and '\' sort before all other characters.  In practice this means that
 // means that directories are sorted naturally before their contents.
 //
 // natural_compare("a/0", "a-b/0") == 1
 // natural_compare_path("a/0", "a-b/0") == -1
int natural_compare_path (Str a, Str b) noexcept;
inline bool natural_lessthan_path (Str a, Str b) {
    return natural_compare_path(a, b) < 0;
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

 // Returns 0 if the given int is not 0..15
constexpr char to_hex_digit (u8 digit) {
    if (digit >= 16) [[unlikely]] return 0;
    return digit + (digit < 10 ? '0' : 'A' - 10);
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

 // Returns the number of decimal digits in the unsigned number.  Can return 1
 // through 20.  You can also think of this as 1+floor(log10(v)) except it
 // returns 1 for 0 instead of -inf.
[[gnu::const]]
u32 count_decimal_digits (u64 v) noexcept;

 // Writes out the decimal form of v.  Count must be the number returned by
 // count_decimal_digits(v).  Returns p + count (the end of the written number).
char* write_decimal_digits (char* p, u32 count, u64 v) noexcept;

 // Like std::from_chars but smaller.  Does not distinguish between error
 // conditions.  Returns {start, 0} if end == start or if the number overflows.
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
        if (r.value < old) return {start, 0};
        r.p++;
    }
    return r;
}

} // namespace uni
