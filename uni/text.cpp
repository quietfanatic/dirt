#include "text.h"

#include <bit>

namespace uni {

ALWAYS_INLINE static
bool digit (char a) { return a >= '0' && a <= '9'; }

 // 0 -> 0
 // / -> 1
 // \ -> 2
 // . -> 3
 // anything else -> 4+ (preserve order)
ALWAYS_INLINE static
i32 adjust (char c) {
     // . and / are one apart so they can be checked in one comparison.
    if (c == '.' || c == '/') {
        return c == '/' ? 1 : 3;
    }
    else if (c == '\\') return 2;
    else [[likely]] return u8(c) << 2; // leave 0 as 0
}

ALWAYS_INLINE static
int normalize (isize v) { return expect(v) < 0 ? -1 : 1; }

int natural_compare (Str a, Str b) noexcept {
     // Use a faster algorithm to find the first difference, then work from
     // there.
    auto ae = a.begin() + (a.size() <= b.size() ? a.size() : b.size());
    auto ap = a.begin() + mem_first_difference(a.begin(), b.begin(), ae - a.begin());
    if (ap == ae) {
         // Didn't find a difference, so the longer side is after.
        return (a.size() > b.size()) - (a.size() < b.size());
    }
     // If one or both sides is not in a number, we can stop early, but we still
     // have to figure out what to return.
    auto bp = b.begin() + (ap - a.begin());
    if (!digit(*ap)) {
         // Neither side is a number.
        if (!digit(*bp)) return normalize(adjust(*ap) - adjust(*bp));
         // a ended the number but b is continuing it, so b has to be after.
        if (ap > a.begin() && digit(ap[-1])) return -1;
         // b is starting a number but a isn't.
        return normalize(adjust(*ap) - adjust(*bp));
    }
    else if (!digit(*bp)) {
         // a is continuing a number but b ended it, so a is larger.
        if (ap > a.begin() && digit(ap[-1])) return 1;
         // a is starting a number but b isn't.
        return normalize(adjust(*ap) - adjust(*bp));
    }
     // Now we have to find the ends of both numbers.  We already know *ap and
     // *bp are digits.
    ae = ap + 1;
    while (ae < a.end() && digit(*ae)) {
        ae += 1;
    }
    auto be = bp + 1;
    while (be < b.end() && digit(*be)) {
        be += 1;
    }
    isize lendiff = (ae - ap) - (be - bp);
     // If the numbers are the same length then we can just pretend we're doing
     // normal string comparison.
    if (lendiff == 0) return normalize(u8(*ap) - u8(*bp));
     // Otherwise we have to find the beginning of the numbers too (we only need
     // to search one of them because we know they're identical up to here).
    auto ab = ap;
    auto bb = bp;
    while (ab > a.begin() && ab[-1] >= '0' && ab[-1] <= '9') {
        ab -= 1;
        bb -= 1;
    }
     // Chop zeroes off the front of the longer number until they're the same
     // length.  If we hit a nonzero, it means the longer number is larger.
     // Only one of these loops will actually run.
    for (ap = ab; ap < ab + lendiff; ap++) {
        if (*ap != '0') return 1;
    }
    for (bp = bb; bp < bb - lendiff; bp++) {
        if (*bp != '0') return -1;
    }
     // Now both numbers are the same length, so compare each digit.
    for (; ap < ae; ap++, bp++) {
        if (*ap != *bp) return normalize(u8(*ap) - u8(*bp));
    }
     // Numbers are equal!  So put the one with more leading zeroes (the longer
     // one) after.  Theoretically lendiff could exceed the range of int, which
     // would be pretty silly, because it means one of our numbers starts with
     // literal billions of zeroes, but oh well.  It's just two extra
     // instructions to deal with that (shift, or).
    return normalize(lendiff);
}

 // There isn't a better way to do this.  I considered a fully-branchless
 // algorithm that uses bitcounting to estimate the digits then a lookup table
 // to refine the count, but it'd require either a 512-byte table or two tables
 // totalling 200ish bytes, and it ended up more complicated for all but the
 // longest numbers (which aren't as common as the shorter numbers).  So we're
 // just using a biased comparison tree instead.  At least we can convert many
 // of the comparisons to branchless form.  I don't really know what the ideal
 // proportion of branches to nonbranches is, but this seems pretty reasonable.
 // If compiled well, the first block fits in 16 bytes of x64 and the first and
 // second together in 64.
u32 count_decimal_digits (u64 v) noexcept {
    if (v <= 9) [[likely]] return 1;
    else if (v <= 99'999) [[likely]] {
        u32 r = 2;
        r += u32(v) > 99;
        r += u32(v) > 999;
        r += u32(v) > 9'999;
        return r;
    }
     // For some reason putting any more [[likely]]s on these makes the compiler
     // merge returns in a suboptimal way (it stops using the eax register?),
     // and I can't convince it not to do that.  Some other perturbations make
     // it do that too, even with only two [[likely]]s.
    else if (v <= 9'999'999'999ULL) {
        u32 r = 6;
        r += u32(v) > 999'999;
        r += u32(v) > 9'999'999;
        r += u32(v) > 99'999'999;
        r += u32(v) > 999'999'999;
        return r;
    }
    else if (v <= 999'999'999'999'999ULL) {
        u32 r = 11;
        r += v > 99'999'999'999ULL;
        r += v > 999'999'999'999ULL;
        r += v > 9'999'999'999'999ULL;
        r += v > 99'999'999'999'999ULL;
        return r;
    }
    else {
        u32 r = 16;
        r += v > 9'999'999'999'999'999ULL;
        r += v > 99'999'999'999'999'999ULL;
        r += v > 999'999'999'999'999'999ULL;
        r += v > 9'999'999'999'999'999'999ULL;
        return r;
    }
}

 // This is short enough it could be inlined, but the caller is likely to
 // already have called count_decimal_digits, so it'll already have a stack
 // frame, so calling one more function won't cost much.
char* write_decimal_digits (char* p, u32 count, u64 v) noexcept {
     // The STL std::to_chars is kinda messy.  It does two digits at a time,
     // which is theoretically faster, but it reads a lookup table and has more
     // instructions and branches, so it's harder on caches and branch tables.
     // Division by a constant is not all that slow.
    expect(count == count_decimal_digits(v));
    expect(count >= 1 && count <= 20);
    if (count == 1) [[likely]] {
        end:
        expect(v < 10);
        *p = '0' + v;
        return p + count;
    }
    for (u32 c = count - 1; c; --c) {
        p[c] = '0' + v % 10;
        v /= 10;
    }
    expect(v);
    goto end;
}

} using namespace uni;

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"
#include "strings.h"

static tap::TestSet tests ("dirt/uni/text", []{
    using namespace tap;
    ok(natural_compare("a", "b") < 0);
    ok(natural_compare("3", "2") > 0);
    ok(natural_compare("a1b", "a10b") < 0);
    ok(natural_compare("a9b", "a10b") < 0);
    ok(natural_compare("a9b", "ab") < 0, "Numbers come before letters");
    ok(natural_compare("9a", "a") < 0, "...including at the beginning");
    ok(natural_compare("a/0a", "a/a") < 0, "...or after a /");
    ok(natural_compare("a0.b", "a.b") > 0, "Numbers are after .");
    ok(natural_compare("a!.b", "a.b") > 0, "! after .");
    ok(natural_compare("a./b", "a.b") < 0, ". after /");
    ok(natural_compare("a\0b", "a/b") < 0, "NUL before /");
    ok(natural_compare("a1b", "a01b") < 0, "More zeroes come after less zeroes");
    ok(natural_compare("a0b", "a00b") < 0, "Including when only zeroes");
    ok(natural_compare("a", "a ") < 0, "Longer comes after");
    ok(natural_compare("a b", "ab") < 0);
    ok(natural_compare("01", "001") < 0);
    ok(natural_compare("a", "あ") < 0, "Put unicode after ascii");
    ok(natural_compare("a/b", "a-b/c") < 0, "natural_compare is directory-aware");
    ok(natural_compare("a\\b", "a-b\\c") < 0, "natural_compare_path accepts \\ as separator");
    ok(natural_compare("a/b", "a\\b") < 0, "\\ is after /");
    ok(natural_compare("v01/a", "v01-v02/a") < 0);
    ok(natural_compare("asdf", "asdf") == 0, "Works on identical strings");
    UniqueString s (5, 0);
    is(count_decimal_digits(52607), 5u, "count_decimal_digits");
    char* p = write_decimal_digits(s.begin(), 5, 52607);
    is(p, s.begin() + 5, "write_decimal_digits length");
    is(s, "52607", "write_decimal_digits contents");
    s = UniqueString(16, 0);
    is(count_decimal_digits(5260715430874368), 16u, "count_decimal_digits");
    p = write_decimal_digits(s.begin(), 16, 5260715430874368);
    is(p, s.begin() + 16, "write_decimal_digits length");
    is(s, "5260715430874368", "write_decimal_digits contents");
    s = UniqueString(2, 0);
    is(count_decimal_digits(0), 1u, "count_decimal_digits");
    p = write_decimal_digits(s.begin(), 1, 0);
    is(p, s.begin() + 1, "write_decimal_digits length");
    is(s, "0\0", "write_decimal_digits contents");
    is(cat("asdf", -48829, "fdsa"), "asdf-48829fdsa", "cat with number");
    done_testing();
});
#endif
