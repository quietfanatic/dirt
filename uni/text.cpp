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
            if (!(*bp >= '0' && *bp <= '9')) return 1;
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
        else if (*bp >= '0' && *bp <= '9') return -1;

        nondigit:
        if (uint8(*ap) != uint8(*bp)) {
            return uint8(*ap) < uint8(*bp) ? -1 : 1;
        }
        ap++; bp++;
    }
     // Ran out of one side, so whichever has more left comes after
    return ap == ae ? -1 : bp == be ? 1 : 0;
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
    is(natural_compare("a1b", "a01b"), -1, "More zeroes come after less zeroes");
    is(natural_compare("a", "a "), -1, "Longer comes after");
    is(natural_compare("a b", "ab"), -1);
    is(natural_compare("01", "001"), -1);
    is(natural_compare("a", "あ"), -1, "Put unicode after ascii");
    done_testing();
});
#endif
