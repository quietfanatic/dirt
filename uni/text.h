// Some convenient text-related functions

#pragma once

#include "common.h"
#include "strings.h"

namespace uni {

 // Does a comparison for a "natural sort", where numbers within the string are
 // sorted by their numeric value regardless of how many digits they are.  The
 // behavior of corner cases may change in future updates.
int natural_compare (Str a, Str b) noexcept;
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

 // Returns 0 if the given int is not 0..15
constexpr char to_hex_digit (uint8 digit) {
    if (digit < 10) return '0' + digit;
    else if (digit < 16) return 'A' + digit - 10;
    else return 0;
}

 // Transforms ' into '\'' for use in bash command arguments.
UniqueString escape_for_shell (Str);

} // namespace uni
