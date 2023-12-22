#include "utf.h"

namespace uni {

 // Yes, I did write my own UTF conversion routines instead of taking a
 // dependency on something else.  The standard library's character conversion
 // system is a mess (more precisely, it's several messes all broken in
 // different ways).
static usize to_utf16_buffer (char16* buffer, Str s) {
    char16* out = buffer;
    const char* end = s.end();
    for (const char* in = s.begin(); in != end; in++) {
        uint8 b0 = in[0];
        if (b0 < 0b1000'0000) {
            *out++ = b0; // ASCII
            continue;
        }
        else if (b0 < 0b1100'0000) goto invalid; // Unmatched continuation
        else if (b0 < 0b1110'0000) {
            if (end - in < 2) goto invalid;  // Truncated sequence
            uint8 b1 = in[1];
            if (b1 < 0b1000'0000 || b1 >= 0b1100'0000) goto invalid;
            uint32 c = (b0 & 0b0001'1111) << 6
                     | (b1 & 0b0011'1111);
            if (c < 0x80) goto invalid;  // Overlong sequence
            *out++ = c;
            in += 1;
            continue;
        }
        else if (b0 < 0b1111'0000) {
            if (end - in < 3) goto invalid;  // Truncated sequence
            uint8 b1 = in[1];
            if (b1 < 0b1000'0000 || b1 >= 0b1100'0000) goto invalid;
            uint8 b2 = in[2];
            if (b2 < 0b1000'0000 || b2 >= 0b1100'0000) goto invalid;
            uint32 c = (b0 & 0b0000'1111) << 12
                     | (b1 & 0b0011'1111) << 6
                     | (b2 & 0b0011'1111);
            if (c < 0x800) goto invalid;  // Overlong sequence
            *out++ = c;
            in += 2;
            continue;
        }
        else if (b0 < 0b1111'1000) {
            if (end - in < 4) goto invalid; // Truncated sequence
            uint8 b1 = in[1];
            if (b1 < 0b1000'0000 || b1 >= 0b1100'0000) goto invalid;
            uint8 b2 = in[2];
            if (b2 < 0b1000'0000 || b2 >= 0b1100'0000) goto invalid;
            uint8 b3 = in[3];
            if (b3 < 0b1000'0000 || b3 >= 0b1100'0000) goto invalid;
            uint32 c = (b0 & 0b0000'0111) << 18
                     | (b1 & 0b0011'1111) << 12
                     | (b2 & 0b0011'1111) << 6
                     | (b3 & 0b0011'1111);
            if (c < 0x10000) goto invalid;  // Overlong sequence
            *out++ = 0xd800 + ((c - 0x10000) >> 10);
            *out++ = 0xdc00 + ((c - 0x10000) & 0x3ff);
            in += 3;
            continue;
        }
        invalid: {
             // Pretend the byte is latin-1 and continue
            *out++ = b0;
        }
    }
    return out - buffer;
}

UniqueString16 to_utf16 (Str s) noexcept {
     // Buffer is not null-terminated
     // Worst-case inflation is 1 code unit (2 bytes) per byte
    usize buffer_size = s.size();
     // We'll say 10k is okay to allocate on the stack
    if (buffer_size < 10000 / sizeof(char16)) {
        char16 buffer [buffer_size];
        usize len = to_utf16_buffer(buffer, s);
        return UniqueString16(buffer, len);
    }
    else {
         // Modern virtual memory systems mean that for big enough allocations,
         // even if we vastly overallocate we won't actually use much more
         // physical RAM than we write to.
        auto buffer = (char16*)malloc(sizeof(char16) * buffer_size);
        usize len = to_utf16_buffer(buffer, s);
        auto r = UniqueString16(buffer, len);
        free(buffer);
        return r;
    }
}

static usize from_utf16_buffer (char* buffer, Str16 s) {
    char* out = buffer;
    const char16* end = s.end();
    for (const char16* in = s.begin(); in != end; in++) {
        uint32 c;
        uint16 u0 = in[0];
        if (u0 < 0xd800 || u0 >= 0xdc00 || in + 1 == end) {
            c = u0;
        }
        else {
            uint16 u1 = in[1];
            if (u1 < 0xdc00 || u1 >= 0xe000) {
                c = u0;
            }
            else {
                c = (u0 - 0xd800) << 10 | (u1 - 0xdc00);
                c += 0x10000;
                in += 1;
            }
        }
        if (c < 0x80) {
            *out++ = c;
        }
        else if (c < 0x800) {
            *out++ = 0b1100'0000 | (c >> 6);
            *out++ = 0b1000'0000 | (c & 0b0011'1111);
        }
        else if (c < 0x10000) {
            *out++ = 0b1110'0000 | (c >> 12);
            *out++ = 0b1000'0000 | (c >> 6 & 0b0011'1111);
            *out++ = 0b1000'0000 | (c & 0b0011'1111);
        }
        else {
            *out++ = 0b1111'0000 | (c >> 18);
            *out++ = 0b1000'0000 | (c >> 12 & 0b0011'1111);
            *out++ = 0b1000'0000 | (c >> 6 & 0b0011'1111);
            *out++ = 0b1000'0000 | (c & 0b0011'1111);
        }
    }
    return out - buffer;
}

UniqueString from_utf16 (Str16 s) noexcept {
     // Buffer is not null-terminated
     // Worst-case inflation is 3 bytes per code unit (1.5x)
    usize buffer_size = s.size() * 3;
     // We'll say 10k is okay to allocate on the stack
    if (buffer_size < 10000 / sizeof(char)) {
        char buffer [buffer_size];
        usize len = from_utf16_buffer(buffer, s);
        return UniqueString(buffer, len);
    }
    else {
         // Modern virtual memory systems mean that for big enough allocations,
         // even if we vastly overallocate we won't actually use much more
         // physical RAM than we write to.
        auto buffer = (char*)malloc(sizeof(char) * buffer_size);
        usize len = from_utf16_buffer(buffer, s);
        auto r = UniqueString(buffer, len);
        free(buffer);
        return r;
    }
}

} using namespace uni;

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

static tap::TestSet tests ("dirt/uni/utf", []{
    using namespace tap;
    is(from_utf16(u"ユニコード"), "ユニコード", "from_utf16");
    is(to_utf16("ユニコード"), u"ユニコード", "to_utf16");
    is(to_utf16("🌱"), u"🌱", "to_utf16 with two-unit character");
    is(from_utf16(u"🌱"), "🌱", "from_utf16 with two-unit character");
     // Assuming little-endian
    is(
        reinterpret_cast<const char*>(to_utf16("ユニコード").c_str()),
        "\xe6\x30\xcb\x30\xb3\x30\xfc\x30\xc9\x30",
        "Actual byte sequence of created utf-16 is correct"
    );
    done_testing();
});
#endif
