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

static UniqueString write_percent (UniqueString s, uint8 c) {
    uint8 high = uint8(c) >> 4;
    uint8 low = uint8(c) & 0xf;
    return cat(move(s), '%',
        char(high >= 10 ? high - 10 + 'A' : high + '0'),
        char(low >= 10 ? low - 10 + 'A' : low + '0')
    );
}
static int read_percent (Str input) {
    if (input.size() < 3) [[unlikely]] return -1;
    uint8 byte = 0;
    for (int i = 1; i <= 2; i++) {
        byte <<= 4;
        switch (input[i]) {
            case IRI_DIGIT: byte |= input[i] - '0'; break;
            case IRI_UPPERHEX: byte |= input[i] - 'A' + 10; break;
            case IRI_LOWERHEX: byte |= input[i] - 'a' + 10; break;
            default: [[unlikely]] return -1;
        }
    }
    return byte;
}
static UniqueString parse_percent (UniqueString s, Str input) {
    int byte = read_percent(input);
    switch (byte) {
        case -1: [[unlikely]] return "";
        case IRI_GENDELIM: case IRI_SUBDELIM:
        case IRI_FORBIDDEN: case IRI_IFFY:
            return write_percent(move(s), byte);
        default: s.push_back(byte); return s;
    }
}

UniqueString encode (Str input) {
    UniqueString r;
    r.reserve(input.size());
    for (size_t i = 0; i < input.size(); i++) {
        switch (input[i]) {
            case IRI_GENDELIM: case IRI_SUBDELIM:
            case IRI_FORBIDDEN: case IRI_IFFY:
            case '%': r = write_percent(move(r), input[i]); break;
            default: r.push_back(input[i]); break;
        }
    }
    return r;
}

UniqueString decode (Str input) {
    UniqueString r;
    r.reserve(input.size());
    for (size_t i = 0; i < input.size(); i++) {
        uint8 byte;
        if (input[i] == '%') {
            int result = read_percent(input.substr(i));
            if (result < 0) return "";
            byte = result;
            i += 2;
        }
        else byte = input[i];
        r.push_back_expect_capacity(byte);
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
    for (size_t i = 1; i < ref.size(); i++) {
        switch (ref[i]) {
            case ':': return Relativity::Scheme;
            case '/': case '?': case '#':
                return Relativity::RelativePath;
            default: break;
        }
    }
    return Relativity::RelativePath;
}

IRI::IRI (Str input, const IRI& base) {
    uint32 i = 0;
    UniqueString spec;
    uint32 colon = 0;
    uint32 path = 0;
    uint32 question = 0;
    uint32 hash = 0;

     // Reject absurdly large input
    if (input.size() > maximum_length) {
        hash_ = uint16(Error::TooLong);
        goto fail;
    }
     // Now start parsing...wait stop.
     // If we've been given a relative reference, we can skip some parsing
    switch (relativity(input)) {
        case Relativity::Scheme: {
             // Optimize for the case that the input won't be altered
            spec.reserve(input.size());
            goto parse_scheme;
        }
        case Relativity::Authority: {
            if (!base) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_with_scheme_only();
            spec.reserve(prefix.size() + input.size());
            spec.append(prefix);
            colon = base.colon_;
            expect(colon + 1 == spec.size());
            goto parse_authority;
        }
        case Relativity::AbsolutePath: {
            if (!base.hierarchical()) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_with_origin_only();
            expect(prefix.size());
            spec.reserve(prefix.size() + input.size());
            spec.append(prefix);
            colon = base.colon_;
            path = base.path_;
            expect(path == spec.size());
            goto parse_path;
        }
        case Relativity::RelativePath: {
            if (!base.hierarchical()) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_without_filename();
            expect(prefix.size());
            spec.reserve(prefix.size() + input.size());
            spec.append(prefix);
            colon = base.colon_;
            path = base.path_;
            expect(path < spec.size());
            goto parse_path;
        }
        case Relativity::Query: {
            if (!base) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_without_query();
            spec.reserve(prefix.size() + input.size());
            spec.append(prefix);
            expect(input[i] == '?');
            spec.push_back('?'); i++;
            colon = base.colon_;
            path = base.path_;
            question = base.question_;
            expect(question + 1 == spec.size());
            goto parse_query;
        }
        case Relativity::Fragment: {
            if (!base) {
                hash_ = uint16(Error::CouldNotResolve);
                goto fail;
            }
            Str prefix = base.spec_without_fragment();
            spec.reserve(prefix.size() + input.size());
            spec.append(prefix);
            expect(input[i] == '#');
            spec.push_back('#'); i++;
            colon = base.colon_;
            path = base.path_;
            question = base.question_;
            hash = base.hash_;
            expect(hash + 1 == spec.size());
            goto parse_fragment;
        }
        default: never();
    }
     // Okay NOW start parsing.

    parse_scheme: while (i < input.size()) {
        switch (input[i]) {
            case IRI_UPPERCASE:
                 // Canonicalize to lowercase
                spec.push_back(input[i++] - 'A' + 'a');
                break;
            case IRI_LOWERCASE:
                spec.push_back(input[i++]);
                break;
            case IRI_DIGIT: case '+': case '-': case '.':
                if (i == 0) {
                    hash_ = uint16(Error::InvalidScheme);
                    goto fail;
                }
                else spec.push_back(input[i++]);
                break;
            case ':':
                if (i == 0) {
                    hash_ = uint16(Error::InvalidScheme);
                    goto fail;
                }
                colon = spec.size();
                spec.push_back(input[i++]);
                goto parse_authority;
            default: {
                hash_ = uint16(Error::InvalidScheme);
                goto fail;
            }
        }
    }
    goto fail;

    parse_authority:
    if (i + 1 < input.size() && input[i] == '/' && input[i+1] == '/') {
        spec.push_back(input[i++]); spec.push_back(input[i++]);
        while (i < input.size()) {
            switch (input[i]) {
                case IRI_UPPERCASE:
                     // Canonicalize to lowercase
                    spec.push_back(input[i++] - 'A' + 'a');
                    break;
                case IRI_LOWERCASE: case IRI_DIGIT:
                case IRI_UNRESERVED_SYMBOL:
                case IRI_UTF8_HIGH:
                case IRI_SUBDELIM:
                case ':': case '[': case ']': case '@':
                    spec.push_back(input[i++]);
                    break;
                case '/':
                    path = spec.size();
                    goto parse_path;
                case '?':
                    path = question = spec.size();
                    spec.push_back(input[i++]);
                    goto parse_query;
                case '#':
                    path = question = hash = spec.size();
                    spec.push_back(input[i++]);
                    goto parse_fragment;
                case '%':
                    spec = parse_percent(move(spec), input.substr(i));
                    if (!spec) {
                        hash_ = uint16(Error::InvalidPercentSequence);
                        goto fail;
                    }
                    i += 3;
                    break;
                case IRI_IFFY:
                    spec = write_percent(move(spec), input[i++]);
                    break;
                default: {
                    hash_ = uint16(Error::InvalidAuthority);
                    goto fail;
                }
            }
        }
        path = question = hash = spec.size();
        goto done;
    }
    else {
         // No authority
        path = i;
    }

    parse_path:
     // We may or may not have the / already.  Kind of an awkward condition but
     // it's less confusing than making sure one or the other case is always
     // true.
    if ((path < spec.size() && spec[path] == '/')
     || (path == spec.size() && i < input.size() && input[i] == '/')) {
         // Canonicalize
        while (i < input.size()) {
            switch (input[i]) {
                case IRI_UPPERCASE: case IRI_LOWERCASE:
                case IRI_DIGIT: case IRI_SUBDELIM:
                case IRI_UTF8_HIGH:
                case '-': case '_': case '~': case ':': case '@':
                    spec.push_back(input[i++]);
                    break;
                case '/': {
                     // Eliminate duplicate /
                    if (spec.back() != '/') {
                        spec.push_back(input[i++]);
                    }
                    else i++;
                    break;
                }
                case '.': {
                    if (spec.back() == '/') {
                        if (i+1 < input.size() && input[i+1] == '.') {
                            if (i+2 == input.size()
                                || input[i+2] == '/'
                                || input[i+2] == '?'
                                || input[i+2] == '#'
                            ) {
                                 // Got a .. so pop off last segment
                                if (spec.size() > path + 1) {
                                    spec.pop_back(); // last slash
                                    while (spec.back() != '/') {
                                        spec.pop_back();
                                    }
                                    i += 2; break;
                                }
                                else {
                                    hash_ = uint16(Error::InvalidPath);
                                    goto fail;
                                }
                            }
                        }
                        else if (i+1 == input.size()
                            || input[i+1] == '/'
                            || input[i+1] == '?'
                            || input[i+1] == '#'
                        ) {
                             // Go a . so ignore it
                            i++; break;
                        }
                    }
                    spec.push_back(input[i++]);
                    break;
                }
                case '?':
                    question = spec.size();
                    spec.push_back(input[i++]);
                    goto parse_query;
                case '#':
                    question = hash = spec.size();
                    spec.push_back(input[i++]);
                    goto parse_fragment;
                case '%':
                    spec = parse_percent(move(spec), input.substr(i));
                    if (!spec) {
                        hash_ = uint16(Error::InvalidPercentSequence);
                        goto fail;
                    }
                    i += 3;
                    break;
                case IRI_IFFY:
                    spec = write_percent(move(spec), input[i++]);
                    break;
                default: {
                    hash_ = uint16(Error::InvalidPath);
                    goto fail;
                }
            }
        }
        question = hash = spec.size();
        goto done;
    }
    else {
         // Doesn't start with / so don't canonicalize
        while (i < input.size()) {
            switch (input[i]) {
                case IRI_UPPERCASE: case IRI_LOWERCASE:
                case IRI_DIGIT: case IRI_SUBDELIM:
                case IRI_UTF8_HIGH:
                case '-': case '_': case '~':
                case ':': case '@': case '/':
                    spec.push_back(input[i++]);
                    break;
                case '?':
                    question = spec.size();
                    spec.push_back(input[i++]);
                    goto parse_query;
                case '#':
                    question = hash = spec.size();
                    spec.push_back(input[i++]);
                    goto parse_fragment;
                case '%':
                    spec = parse_percent(move(spec), input.substr(i));
                    if (!spec) {
                        hash_ = uint16(Error::InvalidPercentSequence);
                        goto fail;
                    }
                    i += 3;
                    break;
                case IRI_IFFY:
                    spec = write_percent(move(spec), input[i++]);
                    break;
                default: {
                    hash_ = uint16(Error::InvalidPath);
                    goto fail;
                }
            }
        }
        question = hash = spec.size();
        goto done;
    }
    parse_query: while (i < input.size()) {
        switch (input[i]) {
            case IRI_UNRESERVED: case IRI_SUBDELIM:
            case ':': case '@': case '/': case '?':
                spec.push_back(input[i++]);
                break;
            case '#':
                hash = spec.size();
                spec.push_back(input[i++]);
                goto parse_fragment;
            case '%':
                spec = parse_percent(move(spec), input.substr(i));
                if (!spec) {
                    hash_ = uint16(Error::InvalidPercentSequence);
                    goto fail;
                }
                i += 3;
                break;
            case IRI_IFFY:
                spec = write_percent(move(spec), input[i++]);
                break;
            default: {
                hash_ = uint16(Error::InvalidQuery);
                goto fail;
            }
        }
    }
    hash = spec.size();
    goto done;

    parse_fragment: while (i < input.size()) {
         // Note that a second # is not allowed.  If that happens, it's likely
         // that there is a nested URL with an unescaped fragment, and in that
         // case it's ambiguous how to parse it, so we won't try.
        switch (input[i]) {
            case IRI_UNRESERVED: case IRI_SUBDELIM:
            case ':': case '@': case '/': case '?':
                spec.push_back(input[i++]);
                break;
            case '%':
                spec = parse_percent(move(spec), input.substr(i));
                if (!spec) {
                    hash_ = uint16(Error::InvalidPercentSequence);
                    goto fail;
                }
                i += 3;
                break;
            case IRI_IFFY:
                spec = write_percent(move(spec), input[i++]);
                break;
            default: {
                hash_ = uint16(Error::InvalidFragment);
                goto fail;
            }
        }
    }
    goto done;

    done: {
        if (spec.size() > maximum_length) {
            hash_ = uint16(Error::TooLong);
            goto fail;
        }
        expect(colon < path);
        expect(colon + 2 != path);
        expect(path <= question);
        expect(question <= hash);
        expect(hash <= spec.size());
        spec_ = move(spec);
        colon_ = colon;
        path_ = path;
        question_ = question;
        hash_ = hash;
        return;
    }
    fail: [[unlikely]] {
        spec_ = input;
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
         // Pulling apart path is NYI
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
};

 // TODO: Add a LOT more tests, this isn't nearly enough.
constexpr TestCase cases [] = {
    {.i = ""},
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
    {.i = "foo"},
    {.i = "foo::", .s = "foo", .p = ":"},
    {.i = "Foo-b+aR://BAR", .s = "foo-b+ar", .a = "bar"},
    {.i = "foo://bar/baz?qux#bap", .s = "foo", .a = "bar", .p = "/baz", .q = "qux", .f = "bap"},
    {.i = "asdf", .b = "foo:bar"},
    {.i = "asdf", .b = "foo:/bar/baz", .s = "foo", .p = "/bar/asdf"},
    {.i = "/asdf", .b = "foo:/bar/baz", .s = "foo", .p = "/asdf"},
    {.i = "../asdf", .b = "foo:/bar/baz", .s = "foo", .p = "/asdf"},
    {.i = "..", .b = "foo:/bar/baz", .s = "foo", .p = "/"},
    {.i = ".", .b = "foo:/bar/baz", .s = "foo", .p = "/bar/"},
    {.i = ".", .b = "foo:/bar/baz/", .s = "foo", .p = "/bar/baz/"},
    {.i = "..", .b = "foo:/bar"},
    {.i = "../..", .b = "foo:/bar/baz/qux/bap", .s = "foo", .p = "/bar/"},
    {.i = "foo://bar/.."},
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
    }
    done_testing();
});

#endif
