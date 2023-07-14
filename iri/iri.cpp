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
    if (in + 3 > end) [[unlikely]] return -1;
    uint8 byte = 0;
    for (int i = 1; i < 3; i++) {
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
    if (!input) return "";
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
            case '%': out = write_percent(out, c); break;
            default: *out++ = c; break;
        }
    }
    r.shrink(out - r.data());
    return r;
}

UniqueString decode (Str input) {
    if (!input) return "";
    const char* in = input.begin();
    const char* end = input.end();
    UniqueString r (input.size(), Uninitialized());
    char* out = r.data();
    while (in != end) {
        if (*in == '%') {
            int result = read_percent(in, end);
            if (result < 0) return "";
            *out++ = result;
            in += 3;
        }
        else *out++ = *in++;
    }
    r.shrink(out - r.data());
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
            case '/': case '?': case '#': return Relativity::RelativePath;
            default: break;
        }
    }
    return Relativity::RelativePath;
}

struct IRIParser {
    Str input;
    UniqueString output;
    uint16 colon;
    uint16 path;
    uint16 question;
    uint16 hash;

    void parse (const IRI& base) {
        if (!input) return fail(Error::Empty);
        if (input.size() > maximum_length) return fail(Error::TooLong);
         // Get capacity upper bound
        usize cap = input.size();
        for (auto c : input) {
            switch (c) {
                case IRI_IFFY: cap += 2; break;
                default: break;
            }
        }
         // Now figure out how much we actually need to parse.
        switch (relativity(input)) {
            case Relativity::Scheme: {
                output.reserve(cap);
                return parse_scheme(
                    output.end(), input.begin(), input.end()
                );
            }
            case Relativity::Authority: {
                if (!base) return fail(Error::CouldNotResolve);
                Str prefix = base.spec_with_scheme_only();
                output.reserve(prefix.size() + cap);
                output.append_expect_capacity(prefix);
                colon = base.colon_;
                return parse_authority(
                    output.end(), input.begin(), input.end()
                );
            }
            case Relativity::AbsolutePath: {
                if (!base.hierarchical()) return fail(Error::CouldNotResolve);
                Str prefix = base.spec_with_origin_only();
                output.reserve(prefix.size() + cap);
                output.append_expect_capacity(prefix);
                colon = base.colon_;
                path = base.path_;
                return parse_absolute_path(
                    output.end(), input.begin(), input.end()
                );
            }
            case Relativity::RelativePath: {
                if (!base.hierarchical()) return fail(Error::CouldNotResolve);
                Str prefix = base.spec_without_filename();
                output.reserve(prefix.size() + cap);
                output.append_expect_capacity(prefix);
                colon = base.colon_;
                path = base.path_;
                return parse_relative_path(
                    output.end(), input.begin(), input.end()
                );
            }
            case Relativity::Query: {
                if (!base) return fail(Error::CouldNotResolve);
                Str prefix = base.spec_without_query();
                output.reserve(prefix.size() + cap);
                output.append_expect_capacity(prefix);
                colon = base.colon_;
                path = base.path_;
                question = base.question_;
                return parse_query(
                    output.end(), input.begin(), input.end()
                );
            }
            case Relativity::Fragment: {
                if (!base) return fail(Error::CouldNotResolve);
                Str prefix = base.spec_without_fragment();
                output.reserve(prefix.size() + cap);
                output.append_expect_capacity(prefix);
                colon = base.colon_;
                path = base.path_;
                question = base.question_;
                hash = base.hash_;
                return parse_fragment(
                    output.end(), input.begin(), input.end()
                );
            }
            default: never();
        }
    }

    void parse_scheme (char* out, const char* in, const char* in_end) {
         // Must start with a letter.
        switch (*in) {
            case IRI_UPPERCASE: *out++ = *in++ - 'A' + 'a'; break;
            case IRI_LOWERCASE: *out++ = *in++; break;
            default: return fail(Error::InvalidScheme);
        }
        while (in < in_end) switch (*in) {
            case IRI_UPPERCASE: *out++ = *in++ - 'A' + 'a'; break;
            case IRI_LOWERCASE: case IRI_DIGIT: case '+': case '-': case '.':
                *out++ = *in++; break;
            case ':':
                colon = out - output.begin();
                *out++ = *in++;
                if (in + 2 <= in_end && in[0] == '/' && in[1] == '/') {
                    return parse_authority(out, in, in_end);
                }
                else {
                    path = out - output.begin();
                    if (in + 1 <= in_end && in[0] == '/') {
                        return parse_absolute_path(out, in, in_end);
                    }
                    else return parse_nonhierarchical_path(out, in, in_end);
                }
            default: return fail(Error::InvalidScheme);
        }
         // We should not have been called if the input doesn't have a :
        never();
    }

    void parse_authority (char* out, const char* in, const char* in_end) {
        expect(out[-1] == ':');
        expect(in + 2 <= in_end && in[0] == '/' && in[1] == '/');
        *out++ = '/'; *out++ = '/';
        in += 2;
        while (in < in_end) switch (*in) {
            case IRI_UPPERCASE: *out++ = *in++ - 'A' + 'a'; break;
            case IRI_LOWERCASE: case IRI_DIGIT:
            case IRI_UNRESERVED_SYMBOL:
            case IRI_UTF8_HIGH:
            case IRI_SUBDELIM:
            case ':': case '[': case ']': case '@':
                *out++ = *in++; break;
            case '/':
                path = out - output.begin();
                return parse_absolute_path(out, in, in_end);
            case '?':
                path = question = out - output.begin();
                return parse_query(out, in, in_end);
            case '#':
                path = question = hash = out - output.begin();
                return parse_fragment(out, in, in_end);
            case '%':
                if (!(out = parse_percent(out, in, in_end))) {
                    return fail(Error::InvalidPercentSequence);
                }
                in += 3; break;
            case IRI_IFFY:
                out = write_percent(out, *in++); break;
            default: return fail(Error::InvalidAuthority);
        }
        path = question = hash = out - output.begin();
        return done(out, in, in_end);
    }

    void parse_absolute_path (char* out, const char* in, const char* in_end) {
        expect(*in == '/');
        *out++ = *in++;
        return parse_relative_path(out, in, in_end);
    }

    void parse_relative_path (char* out, const char* in, const char* in_end) {
        while (in < in_end) switch (*in) {
            case IRI_UPPERCASE: case IRI_LOWERCASE:
            case IRI_DIGIT: case IRI_SUBDELIM:
            case IRI_UTF8_HIGH:
            case '-': case '_': case '~': case ':': case '@': case '.':
                *out++ = *in++; break;
            case '/': case '?': case '#':
                return finish_segment(out, in, in_end);
            case '%':
                if (!(out = parse_percent(out, in, in_end))) {
                    return fail(Error::InvalidPercentSequence);
                }
                in += 3; break;
            case IRI_IFFY:
                out = write_percent(out, *in++); break;
            default: return fail(Error::InvalidPath);
        }
        return finish_segment(out, in, in_end);
    }

    void finish_segment (char* out, const char* in, const char* in_end) {
        if (out[-1] == '.') {
            if (out[-2] == '/') {
                out--;
            }
            else if (out[-2] == '.' && out[-3] == '/') {
                out -= 3;
                if (out - output.begin() == path) {
                    return fail(Error::PathOutsideRoot);
                }
                while (out[-1] != '/') out--;
            }
        }
        if (in < in_end) switch (*in) {
            case '/':
                if (out[-1] == '/') in++;
                else *out++ = *in++;
                return parse_relative_path(out, in, in_end);
            case '?':
                question = out - output.begin();
                return parse_query(out, in, in_end);
            case '#':
                question = hash = out - output.begin();
                return parse_fragment(out, in, in_end);
            default: never();
        }
        question = hash = out - output.begin();
        return done(out, in, in_end);
    }

    void parse_nonhierarchical_path (
        char* out, const char* in, const char* in_end
    ) {
        expect(out[-1] == ':');
        while (in < in_end) switch (*in) {
            case IRI_UPPERCASE: case IRI_LOWERCASE:
            case IRI_DIGIT: case IRI_SUBDELIM:
            case IRI_UTF8_HIGH:
            case '-': case '_': case '~':
            case ':': case '@': case '/':
                *out++ = *in++; break;
            case '?':
                question = out - output.begin();
                return parse_query(out, in, in_end);
            case '#':
                question = hash = out - output.begin();
                return parse_fragment(out, in, in_end);
            case '%':
                if (!(out = parse_percent(out, in, in_end))) {
                    return fail(Error::InvalidPercentSequence);
                }
                in += 3;
                break;
            case IRI_IFFY:
                out = write_percent(out, *in++);
                break;
            default: return fail(Error::InvalidPath);
        }
        question = hash = out - output.begin();
        return done(out, in, in_end);
    }

    void parse_query (char* out, const char* in, const char* in_end) {
        expect(*in == '?');
        *out++ = *in++;
        while (in < in_end) switch (*in) {
            case IRI_UNRESERVED: case IRI_SUBDELIM:
            case ':': case '@': case '/': case '?':
                *out++ = *in++; break;
            case '#':
                hash = out - output.begin();
                return parse_fragment(out, in, in_end);
            case '%':
                if (!(out = parse_percent(out, in, in_end))) {
                    return fail(Error::InvalidPercentSequence);
                }
                in += 3; break;
            case IRI_IFFY:
                out = write_percent(out, *in++); break;
            default: return fail(Error::InvalidQuery);
        }
        hash = out - output.begin();
        return done(out, in, in_end);
    }

    void parse_fragment (char* out, const char* in, const char* in_end) {
        expect(*in == '#');
        *out++ = *in++;
         // Note that a second # is not allowed.  If that happens, it's likely
         // that there is a nested URL with an unescaped fragment, and in that
         // case it's ambiguous how to parse it, so we won't try.
        while (in < in_end) switch (*in) {
            case IRI_UNRESERVED: case IRI_SUBDELIM:
            case ':': case '@': case '/': case '?':
                *out++ = *in++;
                break;
            case '%':
                if (!(out = parse_percent(out, in, in_end))) {
                    return fail(Error::InvalidPercentSequence);
                }
                in += 3;
                break;
            case IRI_IFFY:
                out = write_percent(out, *in++);
                break;
            default: {
                return fail(Error::InvalidFragment);
            }
        }
        return done(out, in, in_end);
    }

    void done (char* out, const char* in, const char* in_end) {
        if (out - output.begin() > maximum_length) return fail(Error::TooLong);
        expect(in == in_end);
        expect(colon < path);
        expect(colon + 2 != path);
        expect(path <= question);
        expect(question <= hash);
        expect(hash <= out - output.begin());
        output.unsafe_set_size(out - output.begin());
    }

    [[gnu::cold]] void fail (Error err) {
        output = input;
        colon = path = question = 0;
        hash = uint16(err);
    }
};

IRI::IRI (Str spec, const IRI& base) {
    IRIParser parser;
    parser.input = spec;
    parser.parse(base);
    spec_ = move(parser.output);
    colon_ = parser.colon;
    path_ = parser.path;
    question_ = parser.question;
    hash_ = parser.hash;
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
    {.i = "foo://bar/baz/", .s = "foo", .a = "bar", .p = "/baz/"},
    {.i = "foo:///bar", .s = "foo", .a = "", .p = "/bar"},
    {.i = "foo:////bar", .s = "foo", .a = "", .p = "/bar"},
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
    {.i = "..", .b = "foo:/bar", .e = Error::PathOutsideRoot},
    {.i = "../..", .b = "foo:/bar/baz/qux/bap", .s = "foo", .p = "/bar/"},
    {.i = "foo://bar/..", .e = Error::PathOutsideRoot},
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
