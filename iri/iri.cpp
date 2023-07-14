#include "iri.h"

#include "../uni/assertions.h"

namespace iri {

#define IRI_UPPERCASE \
         'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': \
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': \
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': \
    case 'V': case 'W': case 'X': case 'Y': case 'Z'
#define IRI_LOWERCASE \
         'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': \
    case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': \
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u': \
    case 'v': case 'w': case 'x': case 'y': case 'z'
#define IRI_DIGIT \
         '0': case '1': case '2': case '3': case '4': \
    case '5': case '6': case '7': case '8': case '9'
#define IRI_UPPERHEX \
         'A': case 'B': case 'C': case 'D': case 'E': case 'F'
#define IRI_LOWERHEX \
         'a': case 'b': case 'c': case 'd': case 'e': case 'f'
#define IRI_GENDELIM \
         ':': case '/': case '?': case '#': case '[': case ']': case '@'
#define IRI_SUBDELIM \
         '!': case '$': case '&': case '\'': case '(': case ')': \
    case '*': case '+': case ',': case ';': case '='
#define IRI_UNRESERVED_SYMBOL \
         '-': case '.': case '_': case '~'
#define IRI_FORBIDDEN \
         0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: \
    case 0x06: case 0x07: case 0x08: case 0x09: case 0x0a: case 0x0b: \
    case 0x0c: case 0x0d: case 0x0e: case 0x0f: \
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: \
    case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: \
    case 0x1c: case 0x1d: case 0x1e: case 0x1f: \
    case 0x20: case 0x7f
#define IRI_IFFY \
         '<': case '>': case '"': case '{': case '}': case '|': case '\\': \
    case '^': case '`'
#define IRI_UTF8_HIGH \
         char(0x80): case char(0x81): case char(0x82): case char(0x83): \
    case char(0x84): case char(0x85): case char(0x86): case char(0x87): \
    case char(0x88): case char(0x89): case char(0x8a): case char(0x8b): \
    case char(0x8c): case char(0x8d): case char(0x8e): case char(0x8f): \
    case char(0x90): case char(0x91): case char(0x92): case char(0x93): \
    case char(0x94): case char(0x95): case char(0x96): case char(0x97): \
    case char(0x98): case char(0x99): case char(0x9a): case char(0x9b): \
    case char(0x9c): case char(0x9d): case char(0x9e): case char(0x9f): \
    case char(0xa0): case char(0xa1): case char(0xa2): case char(0xa3): \
    case char(0xa4): case char(0xa5): case char(0xa6): case char(0xa7): \
    case char(0xa8): case char(0xa9): case char(0xaa): case char(0xab): \
    case char(0xac): case char(0xad): case char(0xae): case char(0xaf): \
    case char(0xb0): case char(0xb1): case char(0xb2): case char(0xb3): \
    case char(0xb4): case char(0xb5): case char(0xb6): case char(0xb7): \
    case char(0xb8): case char(0xb9): case char(0xba): case char(0xbb): \
    case char(0xbc): case char(0xbd): case char(0xbe): case char(0xbf): \
    case char(0xc0): case char(0xc1): case char(0xc2): case char(0xc3): \
    case char(0xc4): case char(0xc5): case char(0xc6): case char(0xc7): \
    case char(0xc8): case char(0xc9): case char(0xca): case char(0xcb): \
    case char(0xcc): case char(0xcd): case char(0xce): case char(0xcf): \
    case char(0xd0): case char(0xd1): case char(0xd2): case char(0xd3): \
    case char(0xd4): case char(0xd5): case char(0xd6): case char(0xd7): \
    case char(0xd8): case char(0xd9): case char(0xda): case char(0xdb): \
    case char(0xdc): case char(0xdd): case char(0xde): case char(0xdf): \
    case char(0xe0): case char(0xe1): case char(0xe2): case char(0xe3): \
    case char(0xe4): case char(0xe5): case char(0xe6): case char(0xe7): \
    case char(0xe8): case char(0xe9): case char(0xea): case char(0xeb): \
    case char(0xec): case char(0xed): case char(0xee): case char(0xef): \
    case char(0xf0): case char(0xf1): case char(0xf2): case char(0xf3): \
    case char(0xf4): case char(0xf5): case char(0xf6): case char(0xf7): \
    case char(0xf8): case char(0xf9): case char(0xfa): case char(0xfb): \
    case char(0xfc): case char(0xfd): case char(0xfe): case char(0xff)
#define IRI_UNRESERVED \
         IRI_UPPERCASE: case IRI_LOWERCASE: case IRI_DIGIT: \
    case IRI_UNRESERVED_SYMBOL: case IRI_UTF8_HIGH

static int read_percent (const char* in, const char* end) {
    if (in + 2 > end) [[unlikely]] return -1;
    uint8 byte = 0;
    for (int i = 0; i < 2; i++) {
        byte <<= 4;
        switch (in[i]) {
            case IRI_DIGIT: byte |= in[i] - '0'; break;
            case IRI_UPPERHEX: byte |= in[i] - 'A' + 10; break;
            case IRI_LOWERHEX: byte |= in[i] - 'a' + 10; break;
            default: [[unlikely]] return -1;
        }
    }
    return byte;
}

static char* write_percent (char* out, uint8 c) {
    uint8 high = uint8(c) >> 4;
    uint8 low = uint8(c) & 0xf;
    *out++ = '%';
    *out++ = high >= 10 ? high - 10 + 'A' : high + '0';
    *out++ = low >= 10 ? low - 10 + 'A' : low + '0';
    return out;
}

static char* parse_percent (char* out, const char* in, const char* end) {
    int byte = read_percent(in, end);
    if (byte < 0) [[unlikely]] return null;
    switch (char(byte)) {
        case IRI_GENDELIM: case IRI_SUBDELIM:
        case IRI_FORBIDDEN: case IRI_IFFY:
            return write_percent(out, byte);
        default: *out++ = byte; return out;
    }
}

UniqueString encode (Str input) {
    usize cap = input.size();
    for (auto c : input) {
        switch (c) {
            case IRI_GENDELIM: case IRI_SUBDELIM:
            case IRI_FORBIDDEN: case IRI_IFFY:
            case '%': cap += 2;
            default: break;
        }
    }
    UniqueString r (cap, Uninitialized());
    char* out = r.data();
    for (auto c : input) {
        switch (c) {
            case IRI_GENDELIM: case IRI_SUBDELIM:
            case IRI_FORBIDDEN: case IRI_IFFY:
            case '%': {
                out = write_percent(out, c); break;
            }
            default: {
                *out++ = c; break;
            }
        }
    }
    r.shrink(out - r.data());
    return r;
}

UniqueString decode (Str input) {
    const char* in = input.begin();
    const char* end = input.end();
    UniqueString r;
    r.reserve(input.size());
    while (in != end) {
        char c = *in++;
        if (c == '%') {
            int result = read_percent(in, end);
            if (result < 0) return "";
            c = result;
            in += 2;
        }
        r.push_back_expect_capacity(c);
    }
    return r;
}

Relativity relativity (Str ref) {
    if (ref.size() == 0) return Relativity::Scheme;
    switch (ref[0]) {
        case ':': return Relativity::Scheme;
        case '/':
            if (ref.size() > 1 && ref[1] == '/') {
                return Relativity::Authority;
            }
            else return Relativity::AbsolutePath;
        case '?': return Relativity::Query;
        case '#': return Relativity::Fragment;
        default: break;
    }
    for (char c : ref) {
        switch (c) {
            case ':': return Relativity::Scheme;
            case '/': case '?': case '#':
                return Relativity::RelativePath;
            default: break;
        }
    }
    return Relativity::RelativePath;
}

usize capacity_upper_bound (Str ref) {
    if (ref.size() > maximum_length) return 0;
    usize r = ref.size();
    for (auto c : ref) {
        switch (c) {
            case IRI_IFFY: r += 2; break;
            default: break;
        }
    }
    return r;
}

IRI::IRI (Str input, const IRI& base) {
    if (!input) return;

    const char* in = input.begin();
    const char* end = input.end();
    char* out;
    usize cap = capacity_upper_bound(input);

     // Reject absurdly large input
    if (input.size() > maximum_length) {
        hash_ = uint16(Error::TooLong);
        goto fail;
    }
     // Now start parsing...wait stop.
     // If we've been given a relative reference, we can skip some parsing
    switch (relativity(input)) {
        case Relativity::Scheme: {
            spec_.reserve(cap);
            out = spec_.mut_data();
            goto parse_scheme;
        }
        case Relativity::Authority: {
            if (!base) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_with_scheme_only();
            spec_.reserve(prefix.size() + cap);
            spec_.append_expect_capacity(prefix);
            colon_ = base.colon_;
            out = spec_.mut_end();
            goto parse_authority;
        }
        case Relativity::AbsolutePath: {
            if (!base.hierarchical()) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_with_origin_only();
            spec_.reserve(prefix.size() + cap);
            spec_.append_expect_capacity(prefix);
            colon_ = base.colon_;
            path_ = base.path_;
            out = spec_.mut_end();
            goto parse_absolute_path;
        }
        case Relativity::RelativePath: {
            if (!base.hierarchical()) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_without_filename();
            spec_.reserve(prefix.size() + cap);
            spec_.append_expect_capacity(prefix);
            colon_ = base.colon_;
            path_ = base.path_;
            out = spec_.mut_end();
            goto parse_relative_path;
        }
        case Relativity::Query: {
            in++;
            if (!base) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_without_query();
            spec_.reserve(prefix.size() + cap);
            spec_.append_expect_capacity(prefix);
            colon_ = base.colon_;
            path_ = base.path_;
            question_ = base.question_;
            out = spec_.mut_end();
            *out++ = '?';
            goto parse_query;
        }
        case Relativity::Fragment: {
            in++;
            if (!base) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_without_fragment();
            spec_.reserve(prefix.size() + cap);
            spec_.append_expect_capacity(prefix);
            colon_ = base.colon_;
            path_ = base.path_;
            question_ = base.question_;
            hash_ = base.hash_;
            out = spec_.mut_end();
            *out++ = '#';
            goto parse_fragment;
        }
        default: never();
    }
     // Okay NOW start parsing.

    parse_scheme:
    {
        char c = *in++;
        switch (c) {
            case IRI_UPPERCASE:
                *out++ = c - 'A' + 'a';
                break;
            case IRI_LOWERCASE:
                *out++ = c;
                break;
            default: {
                hash_ = uint16(Error::InvalidScheme);
                goto fail;
            }
        }
    }
    while (in != end) {
        char c = *in++;
        switch (c) {
            case IRI_UPPERCASE:
                 // Canonicalize to lowercase
                *out++ = c - 'A' + 'a';
                break;
            case IRI_LOWERCASE: case IRI_DIGIT: case '+': case '-': case '.':
                *out++ = c;
                break;
            case ':':
                colon_ = out - spec_.begin();
                *out++ = ':';
                goto parse_authority;
            default: {
                hash_ = uint16(Error::InvalidScheme);
                goto fail;
            }
        }
    }
     // If the input doesn't have a :, we should never have been sent to
     // parse_scheme.
    never();

    parse_authority: expect(out[-1] == ':');
    if (in + 2 <= end && in[0] == '/' && in[1] == '/') {
        *out++ = '/'; *out++ = '/';
        in += 2;
        while (in != end) {
            char c = *in++;
            switch (c) {
                case IRI_UPPERCASE:
                     // Canonicalize to lowercase
                    *out++ = c - 'A' + 'a';
                    break;
                case IRI_LOWERCASE: case IRI_DIGIT:
                case IRI_UNRESERVED_SYMBOL:
                case IRI_UTF8_HIGH:
                case IRI_SUBDELIM:
                case ':': case '[': case ']': case '@':
                    *out++ = c;
                    break;
                case '/':
                    path_ = out - spec_.begin();
                    in--;
                    goto parse_absolute_path;
                case '?':
                    path_ = question_ = out - spec_.begin();
                    *out++ = '?';
                    goto parse_query;
                case '#':
                    path_ = question_ = hash_ = out - spec_.begin();
                    *out++ = '#';
                    goto parse_fragment;
                case '%':
                    if (!(out = parse_percent(out, in, end))) {
                        hash_ = uint16(Error::InvalidPercentSequence);
                        goto fail;
                    }
                    in += 2;
                    break;
                case IRI_IFFY:
                    out = write_percent(out, c);
                    break;
                default: {
                    hash_ = uint16(Error::InvalidAuthority);
                    goto fail;
                }
            }
        }
        path_ = question_ = hash_ = out - spec_.begin();
        goto done;
    }
    else {
         // No authority
        path_ = out - spec_.begin();
        if (in != end && *in == '/') {
            goto parse_absolute_path;
        }
        else goto parse_nonhierarchical_path;
    }

    parse_absolute_path:
    *out++ = '/'; in++;

    parse_relative_path:
    while (in < end) {
        char c = *in++;
        switch (c) {
            case IRI_UPPERCASE: case IRI_LOWERCASE:
            case IRI_DIGIT: case IRI_SUBDELIM:
            case IRI_UTF8_HIGH:
            case '-': case '_': case '~': case ':': case '@':
                *out++ = c;
                break;
            case '/': {
                 // Eliminate duplicate /
                if (out[-1] != '/') {
                    *out++ = c;
                }
                else in++;
                break;
            }
            case '.': {
                if (out[-1] == '/') {
                    if (in < end && *in == '.') {
                        if (in+1 == end ||
                            in[1] == '/' || in[1] == '?' || in[1] == '#'
                        ) {
                             // Got a .. so pop off last segment
                            if (out - spec_.begin() - 1 > path_) {
                                out--; // last slash
                                while (out[-1] != '/') {
                                    out--;
                                }
                                in += 2;
                                break;
                            }
                            else {
                                hash_ = uint16(Error::InvalidPath);
                                goto fail;
                            }
                        }
                    }
                    else if (
                        in == end || *in == '/' || *in == '?' || *in == '#'
                    ) {
                         // Go a . so ignore it
                        in++;
                        break;
                    }
                }
                *out++ = c;
                break;
            }
            case '?':
                question_ = out - spec_.begin();
                *out++ = '?';
                goto parse_query;
            case '#':
                question_ = hash_ = out - spec_.begin();
                *out++ = '#';
                goto parse_fragment;
            case '%':
                if (!(out = parse_percent(out, in, end))) {
                    hash_ = uint16(Error::InvalidPercentSequence);
                    goto fail;
                }
                in += 2;
                break;
            case IRI_IFFY:
                out = write_percent(out, c);
                break;
            default: {
                hash_ = uint16(Error::InvalidPath);
                goto fail;
            }
        }
    }
    question_ = hash_ = out - spec_.begin();
    goto done;

    parse_nonhierarchical_path: expect(out[-1] == ':');
    while (in != end) {
        char c = *in++;
        switch (c) {
            case IRI_UPPERCASE: case IRI_LOWERCASE:
            case IRI_DIGIT: case IRI_SUBDELIM:
            case IRI_UTF8_HIGH:
            case '-': case '_': case '~':
            case ':': case '@': case '/':
                *out++ = c;
                break;
            case '?':
                question_ = out - spec_.begin();
                *out++ = '?';
                goto parse_query;
            case '#':
                question_ = hash_ = out - spec_.begin();
                *out++ = '#';
                goto parse_fragment;
            case '%':
                if (!(out = parse_percent(out, in, end))) {
                    hash_ = uint16(Error::InvalidPercentSequence);
                    goto fail;
                }
                in += 2;
                break;
            case IRI_IFFY:
                out = write_percent(out, c);
                break;
            default: {
                hash_ = uint16(Error::InvalidPath);
                goto fail;
            }
        }
    }
    question_ = hash_ = out - spec_.begin();
    goto done;

    parse_query: expect(out[-1] == '?');
    while (in != end) {
        char c = *in++;
        switch (c) {
            case IRI_UNRESERVED: case IRI_SUBDELIM:
            case ':': case '@': case '/': case '?':
                *out++ = c;
                break;
            case '#':
                hash_ = out - spec_.begin();
                *out++ = '#';
                goto parse_fragment;
            case '%':
                if (!(out = parse_percent(out, in, end))) {
                    hash_ = uint16(Error::InvalidPercentSequence);
                    goto fail;
                }
                in += 2;
                break;
            case IRI_IFFY:
                out = write_percent(out, c);
                break;
            default: {
                hash_ = uint16(Error::InvalidQuery);
                goto fail;
            }
        }
    }
    hash_ = out - spec_.begin();
    goto done;

    parse_fragment: expect(out[-1] == '#');
    while (in != end) {
         // Note that a second # is not allowed.  If that happens, it's likely
         // that there is a nested URL with an unescaped fragment, and in that
         // case it's ambiguous how to parse it, so we won't try.
        char c = *in++;
        switch (c) {
            case IRI_UNRESERVED: case IRI_SUBDELIM:
            case ':': case '@': case '/': case '?':
                *out++ = c;
                break;
            case '%':
                if (!(out = parse_percent(out, in, end))) {
                    hash_ = uint16(Error::InvalidPercentSequence);
                    goto fail;
                }
                in += 2;
                break;
            case IRI_IFFY:
                out = write_percent(out, c);
                break;
            default: {
                hash_ = uint16(Error::InvalidFragment);
                goto fail;
            }
        }
    }
    goto done;

    done: {
        if (out - spec_.begin() > maximum_length) {
            hash_ = uint16(Error::TooLong);
            goto fail;
        }
//        expect(in == end);
        expect(colon_ < path_);
        expect(colon_ + 2 != path_);
        expect(path_ <= question_);
        expect(question_ <= hash_);
        expect(hash_ <= out - spec_.begin());
        spec_.unsafe_set_size(out - spec_.begin());
        return;
    }
    fail: [[unlikely]] {
        spec_ = input;
        colon_ = path_ = question_ = 0;
        expect(hash_ != 0);
        return;
    }
}

AnyString IRI::spec_relative_to (const IRI& base) const {
    if (!*this) return "";
    else if (!base) return spec_;
    else if (scheme() != base.scheme()) return spec_;
    else if (authority() != base.authority()) {
        if (has_authority()) return spec_.substr(colon_ + 1);
        else return spec_;
    }
    else if (path() != base.path()) {
         // Pulling apart path is NYI. TODO: make it YYI
        return spec_.substr(path_);
    }
    else if (query() != base.query()) {
        return spec_.substr(question_);
    }
    else {
         // Wait!  We're not allowed to return an empty string, so only chop off
         // as much as we can.
        if (hash_ != spec_.size()) {
            return spec_.substr(hash_);
        }
        else if (question_ != spec_.size()) {
            return spec_.substr(question_);
        }
        else if (path_ != spec_.size()) {
            return spec_.substr(path_);
        }
        else if (colon_ + 1u != spec_.size()) {
            return spec_.substr(colon_ + 1);
        }
        else return spec_;
    }
}

} using namespace iri;

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

namespace iri::test {

struct TestCase {
    Str i = "";
    Str b = "";
    Str s = "";
    Str a = "";
    Str p = "";
    Str q = "";
    Str f = "";
    Error e = Error::NoError;
};

 // TODO: Add a LOT more tests, this isn't nearly enough.
constexpr TestCase cases [] = {
    {.i = "", .e = Error::Empty},
    {.i = "foo:", .s = "foo"},
    {.i = "foo:/", .s = "foo", .p = "/"},
    {.i = "foo://", .s = "foo", .a = ""},
    {.i = "foo:bar", .s = "foo", .p = "bar"},
    {.i = "foo:/bar", .s = "foo", .p = "/bar"},
    {.i = "foo://bar", .s = "foo", .a = "bar"},
    {.i = "foo://bar/", .s = "foo", .a = "bar", .p = "/"},
    {.i = "foo://bar/baz", .s = "foo", .a = "bar", .p = "/baz"},
    {.i = "foo:?bar", .s = "foo", .q = "bar"},
    {.i = "foo:#bar", .s = "foo", .f = "bar"},
    {.i = "foo", .e = Error::CouldNotResolve},
    {.i = "foo::", .s = "foo", .p = ":"},
    {.i = "Foo-b+aR://BAR", .s = "foo-b+ar", .a = "bar"},
    {.i = "foo://bar/baz?qux#bap", .s = "foo", .a = "bar", .p = "/baz", .q = "qux", .f = "bap"},
    {.i = "asdf", .b = "foo:bar", .e = Error::CouldNotResolve},
    {.i = "asdf", .b = "foo:/bar/baz", .s = "foo", .p = "/bar/asdf"},
    {.i = "/asdf", .b = "foo:/bar/baz", .s = "foo", .p = "/asdf"},
    {.i = "../asdf", .b = "foo:/bar/baz", .s = "foo", .p = "/asdf"},
    {.i = "..", .b = "foo:/bar/baz", .s = "foo", .p = "/"},
    {.i = ".", .b = "foo:/bar/baz", .s = "foo", .p = "/bar/"},
    {.i = ".", .b = "foo:/bar/baz/", .s = "foo", .p = "/bar/baz/"},
    {.i = "..", .b = "foo:/bar", .e = Error::InvalidPath},
    {.i = "../..", .b = "foo:/bar/baz/qux/bap", .s = "foo", .p = "/bar/"},
    {.i = "foo://bar/..", .e = Error::InvalidPath},
    {.i = "foo:/bar/baz/..", .s = "foo", .p = "/bar/"},
    {.i = "?bar", .b = "foo:", .s = "foo", .q = "bar"},
    {.i = "#bar", .b = "foo:", .s = "foo", .f = "bar"},
    {.i = "?bar", .b = "foo:?baz#qux", .s = "foo", .q = "bar"},
    {.i = "#bar", .b = "foo:?baz#qux", .s = "foo", .q = "baz", .f = "bar"},
    {.i = "foo:/ユニコード", .s = "foo", .p = "/ユニコード"},
    {.i = "foo://ユ/ニ?コー#ド", .s = "foo", .a = "ユ", .p = "/ニ", .q = "コー", .f = "ド"},
    {.i = "ayu-test:/#bar/1/bu%2Fp//33/0/'3/''/'//", .s = "ayu-test", .p = "/", .f = "bar/1/bu%2Fp//33/0/'3/''/'//"},
};
constexpr auto n_cases = sizeof(cases) / sizeof(cases[0]);

} // namespace iri::test

static tap::TestSet tests ("dirt/iri/iri", []{
    using namespace tap;
    using namespace iri::test;
    IRI empty;
    ok(!empty.valid(), "!empty.valid()");
    ok(empty.empty(), "empty.empty()");
    ok(!empty, "!empty");
    for (uint32 i = 0; i < n_cases; i++) {
        IRI iri (cases[i].i, IRI(cases[i].b));
        is(iri.scheme(), cases[i].s, cat(
            cases[i].i, " (", cases[i].b, ") scheme = ", cases[i].s
        ));
        is(iri.authority(), cases[i].a, cat(
            cases[i].i, " (", cases[i].b, ") authority = ", cases[i].a
        ));
        is(iri.path(), cases[i].p, cat(
            cases[i].i, " (", cases[i].b, ") path = ", cases[i].p
        ));
        is(iri.query(), cases[i].q, cat(
            cases[i].i, " (", cases[i].b, ") query = ", cases[i].q
        ));
        is(iri.fragment(), cases[i].f, cat(
            cases[i].i, " (", cases[i].b, ") fragment = ", cases[i].f
        ));
        is(iri.error(), cases[i].e, cat(
            cases[i].i, " (", cases[i].b, ") error = ", uint16(cases[i].e)
        ));
    }
    done_testing();
});

#endif
