#include "text.h"

namespace uni {

 // From what I see, different implementations of natural sort vary on their
 // behavior in corner cases.  For example:
 //     ls -v      |   nemo
 //   "001" < "01" | "01" < "001"
 //   "ab" < "a "  | "a " < "ab"
 // I'm going to side with nemo's behavior because it looks easier. : )
int natural_compare (Str a, Str b) noexcept {
    auto ap = a.begin();
    auto ae = a.end();
    auto bp = b.begin();
    auto be = b.end();
    while (ap != ae && bp != be) {
         // If one has a number but not the other, the number comes afterwards.
         // e.g. image.png before image2.png
        if (*ap >= '0' && *ap <= '9') {
            if (!(*bp >= '0' && *bp <= '9')) {
                 // Unless the number is at the beginning.  It seems to be
                 // expected that filenames starting with numbers come first.
                if (ap == a.begin() || ap[-1] == '/' || ap[-1] == '\\') {
                    return -1;
                }
                return 1;
            }
             // Skip zeroes
            auto az = ap;
            auto bz = bp;
            while (ap != ae && *ap == '0') ap++;
            while (bp != be && *bp == '0') bp++;
             // Capture run of digits
            auto an = ap;
            auto bn = bp;
            while (ap != ae && *ap >= '0' && *ap <= '9') ap++;
            while (bp != be && *bp >= '0' && *bp <= '9') bp++;
             // If there are more digits (after zeros), it comes after
            if (ap - an != bp - bn) {
                return ap - an < bp - bn ? -1 : 1;
            }
             // Otherwise, compare digits in the number
            while (an != ap) {
                if (uint8(*an) != uint8(*bn)) {
                    return uint8(*an) < uint8(*bn) ? -1 : 1;
                }
                an++; bn++;
            }
             // Digits are the same, so if there are more zeros put it after
            if (ap - az != bp - bz) {
                return ap - az < bp - bz ? -1 : 1;
            }
            if (ap == ae) {
                return bp == be ? 0 : -1;
            }
            else if (bp == be) return 1;
             // Zeros and digits are the same so continue with nondigits
            goto nondigit;
        }
        else if (*bp >= '0' && *bp <= '9') {
            if (ap == a.begin() || ap[-1] == '/' || ap[-1] == '\\') {
                return 1;
            }
            else return -1;
        }

        nondigit:
        if (uint8(*ap) != uint8(*bp)) {
            return uint8(*ap) < uint8(*bp) ? -1 : 1;
        }
        ap++; bp++;
    }
     // Ran out of one side, so whichever has more left comes after
    return ap == ae ? -1 : bp == be ? 1 : 0;
}

uint8 count_decimal_digits (uint64 v) {
     // There isn't a better way to do this.  You can estimate the digits with
     // some branchless logarithmic math, but you can't really get a precise
     // count that way.
    if (v <= 9) [[likely]] return 1;
    if (v <= 99) return 2;
    if (v <= 999) return 3;
    if (v <= 9'999) return 4;
    if (v <= 99'999) return 5;
    if (v <= 999'999) return 6;
    if (v <= 9'999'999) return 7;
    if (v <= 99'999'999) return 8;
    if (v <= 999'999'999) return 9;
    if (v <= 9'999'999'999ULL) return 10;
    if (v <= 99'999'999'999ULL) return 11;
    if (v <= 999'999'999'999ULL) return 12;
    if (v <= 9'999'999'999'999ULL) return 13;
    if (v <= 99'999'999'999'999ULL) return 14;
    if (v <= 999'999'999'999'999ULL) return 15;
    if (v <= 9'999'999'999'999'999ULL) return 16;
    if (v <= 99'999'999'999'999'999ULL) return 17;
    if (v <= 999'999'999'999'999'999ULL) return 18;
    if (v <= 9'999'999'999'999'999'999ULL) return 19;
    return 20;
}

char* write_decimal_digits (char* p, uint8 count, uint64 v) {
    expect(count > 0 && count <= 20);
    for (uint8 c = count; c; --c) {
        p[c-1] = '0' + v % 10;
        v /= 10;
    }
    return p + count;
}

} using namespace uni;

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

static tap::TestSet tests ("dirt/uni/text", []{
    using namespace tap;
    is(natural_compare("a", "b"), -1);
    is(natural_compare("3", "2"), 1);
    is(natural_compare("a1b", "a10b"), -1);
    is(natural_compare("a9b", "a10b"), -1);
    is(natural_compare("a9b", "ab"), 1, "Numbers come after no numbers");
    is(natural_compare("9a", "a"), -1, "...unless the number is at the beginning");
    is(natural_compare("a/0a", "a/a"), -1, "...or after a /");
    is(natural_compare("a1b", "a01b"), -1, "More zeroes come after less zeroes");
    is(natural_compare("a", "a "), -1, "Longer comes after");
    is(natural_compare("a b", "ab"), -1);
    is(natural_compare("01", "001"), -1);
    is(natural_compare("a", "あ"), -1, "Put unicode after ascii");
    UniqueString s (5, 0);
    is(count_decimal_digits(52607), 5, "count_decimal_digits");
    char* p = write_decimal_digits(s.begin(), 5, 52607);
    is(p, s.begin() + 5, "write_decimal_digits length");
    is(s, "52607", "write_decimal_digits contents");
    s = UniqueString(16, 0);
    is(count_decimal_digits(5260715430874368), 16, "count_decimal_digits");
    p = write_decimal_digits(s.begin(), 16, 5260715430874368);
    is(p, s.begin() + 16, "write_decimal_digits length");
    is(s, "5260715430874368", "write_decimal_digits contents");
    s = UniqueString(2, 0);
    is(count_decimal_digits(0), 1, "count_decimal_digits");
    p = write_decimal_digits(s.begin(), 1, 0);
    is(p, s.begin() + 1, "write_decimal_digits length");
    is(s, "0\0", "write_decimal_digits contents");
    done_testing();
});
#endif
