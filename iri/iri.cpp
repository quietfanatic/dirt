#include "iri.h"

#include "../uni/assertions.h"
#include "../uni/strings.h"

namespace iri {
using namespace in;

static int read_percent (const char* in, const char* end) {
    if (in + 3 > end) [[unlikely]] return -1;
    int byte = 0;
    for (int i = 1; i < 3; i++) {
        byte <<= 4;
        u32 c = in[i];
        if (!char_is_hexadecimal(c)) [[unlikely]] return -1;
        u32 d = c - '0';
        u32 h = (c | 0x20) - 'a' + 10;
        u32 n = d < 10 ? d : h;
        byte |= n;
    }
    return byte;
}

NOINLINE static
char* write_percent (char* out, u8 c) {
    u8 high = u8(c) >> 4;
    u8 low = u8(c) & 0xf;
    *out++ = '%';
    *out++ = high >= 10 ? high - 10 + 'A' : high + '0';
    *out++ = low >= 10 ? low - 10 + 'A' : low + '0';
    return out;
}

NOINLINE static
char* canonicalize_percent (char* out, const char* in, const char* in_end) {
    int byte = read_percent(in, in_end);
    if (byte < 0) [[unlikely]] return null;
    if (char_wants_encode(byte)) {
        return write_percent(out, byte);
    }
    else {
        *out++ = byte;
        return out;
    }
}

UniqueString encode (Str input) noexcept {
    if (!input) [[unlikely]] return "";
    usize cap = input.size();
    expect(input.begin() < input.end());
    for (auto c : input) {
        cap += char_wants_encode(c) * 2;
    }
    expect(cap > 0);
    char* buf = SharableBuffer<char>::allocate(cap);
    char* out = buf;
    for (auto c : input) {
        if (char_wants_encode(c)) {
            out = write_percent(out, c);
        }
        else *out++ = c;
    }
    UniqueString r;
    r.impl = {u32(out - buf), buf};
    return r;
}

UniqueString decode (Str input) noexcept {
    if (!input) [[unlikely]] return "";
    const char* in = input.begin();
    const char* end = input.end();
    char* buf = SharableBuffer<char>::allocate(input.size());
    char* out = buf;
    expect(in != end);
    while (in != end) {
        if (*in == '%') {
            int result = read_percent(in, end);
            if (result < 0) [[unlikely]] {
                SharableBuffer<char>::deallocate(buf);
                return "";
            }
            *out++ = result;
            in += 3;
        }
        else *out++ = *in++;
    }
    UniqueString r;
    r.impl = {u32(out - buf), buf};
    return r;
}

struct IRIParser {
    const char* in_end;
    const char* in_begin;
    UniqueString output;
    u16 scheme_end;
    u16 authority_end;
    u16 path_end;
    u16 query_end;

    void parse (Str ref, const IRI& base) {
        expect(ref);
        in_begin = ref.begin();
        in_end = ref.end();
        if (ref.size() > maximum_length) return fail(Error::TooLong);
         // Figure out how much to allocate
        usize cap = ref.size();
        for (auto c : ref) {
            if (char_behavior(c) == CharProps::Iffy) cap += 2;
        }
        expect(cap > 0 && cap <= maximum_length * 3);
        switch (relativity(ref)) {
            case Relativity::Scheme: {
                expect(!output.owned());
                new (&output) UniqueString(Capacity(cap));
                return parse_scheme(output.begin(), in_begin);
            }
            case Relativity::Authority: {
                if (!base) return fail(Error::CouldNotResolve);
                scheme_end = base.scheme_end;
                char* out = copy_prefix(base.spec_.slice(0, base.scheme_end + 1), cap);
                return parse_authority(out, in_begin);
            }
            case Relativity::AbsolutePath: {
                if (!base || base.nonhierarchical()) {
                    return fail(Error::CouldNotResolve);
                }
                scheme_end = base.scheme_end;
                authority_end = base.authority_end;
                char* out = copy_prefix(base.spec_.slice(0, base.authority_end), cap);
                return parse_absolute_path(out, in_begin);
            }
            case Relativity::RelativePath: {
                if (!base.hierarchical()) return fail(Error::CouldNotResolve);
                usize i = base.path_end;
                while (base.spec_[i-1] != '/') --i;
                scheme_end = base.scheme_end;
                authority_end = base.authority_end;
                char* out = copy_prefix(base.spec_.slice(0, i), cap);
                return parse_relative_path(out, in_begin);
            }
            case Relativity::Query: {
                if (!base) return fail(Error::CouldNotResolve);
                scheme_end = base.scheme_end;
                authority_end = base.authority_end;
                path_end = base.path_end;
                char* out = copy_prefix(base.spec_.slice(0, base.path_end), cap);
                return parse_query(out, in_begin);
            }
            case Relativity::Fragment: {
                if (!base) return fail(Error::CouldNotResolve);
                scheme_end = base.scheme_end;
                authority_end = base.authority_end;
                path_end = base.path_end;
                query_end = base.query_end;
                char* out = copy_prefix(base.spec_.slice(0, base.query_end), cap);
                return parse_fragment(out, in_begin);
            }
            default: never();
        }
    }

    NOINLINE
    char* copy_prefix (Str prefix, usize cap) {
         // Allocate and start
        expect(!output.owned());
        usize newcap = prefix.size() + cap;
        expect(newcap > 0 && newcap <= maximum_length * 4);
        new (&output) UniqueString(Capacity(newcap));
        char* out = (char*)std::memcpy(output.data(), prefix.data(), prefix.size());\
        return out + prefix.size();
    }

    void parse_scheme (char* out, const char* in) {
         // Must start with a letter.
        if (!char_scheme_valid_start(*in)) return fail(Error::SchemeInvalid);
        *out++ = *in++ | ('a' & ~'A');
        while (in < in_end) {
            if (*in == ':') {
                scheme_end = out - output.begin();
                *out++ = *in++;
                if (in + 2 <= in_end && in[0] == '/' && in[1] == '/') {
                    return parse_authority(out, in);
                }
                else {
                    authority_end = out - output.begin();
                    if (in + 1 <= in_end && in[0] == '/') {
                        return parse_absolute_path(out, in);
                    }
                    else return parse_nonhierarchical_path(out, in);
                }
            }
            else if (char_scheme_valid(*in)) {
                *out++ = *in++ | ('a' & ~'A');
            }
            else return fail(Error::SchemeInvalid);
        }
         // We should not have been called if the input doesn't have a :
        never();
    }

    NOINLINE
    void parse_authority (char* out, const char* in) {
        expect(out[-1] == ':');
        expect(in + 2 <= in_end && Str(in, 2) == "//");
        *out++ = '/'; *out++ = '/';
        in += 2;
        while (in < in_end) switch (char_behavior(*in)) {
            case CharProps::Ordinary: {
                char c = *in++;
                if (c >= 'A' && c <= 'Z') [[unlikely]] {
                    c += 'a' - 'A';
                }
                *out++ = c;
                break;
            }
            case CharProps::Slash:
                authority_end = out - output.begin();
                return parse_absolute_path(out, in);
            case CharProps::Question:
                authority_end = path_end = out - output.begin();
                return parse_query(out, in);
            case CharProps::Hash:
                authority_end = path_end = query_end = out - output.begin();
                return parse_fragment(out, in);
            case CharProps::Percent:
                if (!(out = canonicalize_percent(out, in, in_end))) {
                    return fail(Error::PercentSequenceInvalid);
                }
                in += 3; break;
            case CharProps::Iffy:
                out = write_percent(out, *in++); break;
            case CharProps::Forbidden:
                return fail(Error::AuthorityInvalid);
            default: never();
        }
        authority_end = path_end = query_end = out - output.begin();
        return done(out, in);
    }

    void parse_absolute_path (char* out, const char* in) {
        expect(*in == '/');
        *out++ = *in++;
        return parse_relative_path(out, in);
    }

    NOINLINE
    void parse_relative_path (char* out, const char* in) {
        while (in < in_end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
                *out++ = *in++; break;
            case CharProps::Slash:
                if (!(out = collapse_segment(out))) {
                    return fail(Error::PathOutsideRoot);
                }
                 // Only here can we collapse extra /s without accidentally
                 // chopping off a final /
                if (out[-1] == '/') in++;
                else *out++ = *in++;
                break;
            case CharProps::Question:
                if (!(out = collapse_segment(out))) {
                    return fail(Error::PathOutsideRoot);
                }
                path_end = out - output.begin();
                return parse_query(out, in);
            case CharProps::Hash:
                if (!(out = collapse_segment(out))) {
                    return fail(Error::PathOutsideRoot);
                }
                path_end = query_end = out - output.begin();
                return parse_fragment(out, in);
            case CharProps::Percent:
                if (!(out = canonicalize_percent(out, in, in_end))) {
                    return fail(Error::PercentSequenceInvalid);
                }
                in += 3; break;
            case CharProps::Iffy:
                out = write_percent(out, *in++); break;
            case CharProps::Forbidden:
                return fail(Error::PathInvalid);
            default: never();
        }
        if (!(out = collapse_segment(out))) {
            return fail(Error::PathOutsideRoot);
        }
        path_end = query_end = out - output.begin();
        return done(out, in);
    }

    char* collapse_segment (char* out) {
        if (Str(out-3, 3) == "/..") {
            out -= 3;
            if (out - output.begin() == authority_end)
                [[unlikely]] return null;
            while (out[-1] != '/') out--;
        }
        else if (Str(out-2, 2) == "/.") {
            out--;
        }
        return out;
    }

    NOINLINE
    void parse_nonhierarchical_path (char* out, const char* in) {
        expect(out[-1] == ':');
        while (in < in_end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
            case CharProps::Slash:
                *out++ = *in++; break;
            case CharProps::Question:
                path_end = out - output.begin();
                return parse_query(out, in);
            case CharProps::Hash:
                path_end = query_end = out - output.begin();
                return parse_fragment(out, in);
            case CharProps::Percent:
                if (!(out = canonicalize_percent(out, in, in_end))) {
                    return fail(Error::PercentSequenceInvalid);
                }
                in += 3; break;
            case CharProps::Iffy:
                out = write_percent(out, *in++);
                break;
            case CharProps::Forbidden:
                return fail(Error::PathInvalid);
            default: never();
        }
        path_end = query_end = out - output.begin();
        return done(out, in);
    }

    NOINLINE
    void parse_query (char* out, const char* in) {
        expect(*in == '?');
        *out++ = *in++;
        while (in < in_end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
            case CharProps::Slash:
            case CharProps::Question:
                *out++ = *in++; break;
            case CharProps::Hash:
                query_end = out - output.begin();
                return parse_fragment(out, in);
            case CharProps::Percent:
                if (!(out = canonicalize_percent(out, in, in_end))) {
                    return fail(Error::PercentSequenceInvalid);
                }
                in += 3; break;
            case CharProps::Iffy:
                out = write_percent(out, *in++); break;
            case CharProps::Forbidden:
                return fail(Error::QueryInvalid);
            default: never();
        }
        query_end = out - output.begin();
        return done(out, in);
    }

    NOINLINE
    void parse_fragment (char* out, const char* in) {
        expect(*in == '#');
        *out++ = *in++;
         // Note that a second # is not allowed.  If that happens, it's likely
         // that there is a nested URL with an unescaped fragment, and in that
         // case it's ambiguous how to parse it, so we won't try.
        while (in < in_end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
            case CharProps::Slash:
            case CharProps::Question:
                *out++ = *in++;
                break;
            case CharProps::Percent:
                if (!(out = canonicalize_percent(out, in, in_end))) {
                    return fail(Error::PercentSequenceInvalid);
                }
                in += 3; break;
            case CharProps::Iffy:
                out = write_percent(out, *in++);
                break;
            case CharProps::Forbidden:
            case CharProps::Hash:
                return fail(Error::FragmentInvalid);
            default: never();
        }
        return done(out, in);
    }

    void done (char* out, const char* in) {
        if (out - output.begin() > maximum_length) return fail(Error::TooLong);
        expect(in == in_end);
        expect(scheme_end < authority_end);
        expect(scheme_end + 2 != authority_end);
        expect(authority_end <= path_end);
        expect(path_end <= query_end);
        expect(query_end <= out - output.begin());
        output.impl.size = out - output.begin();
    }

    [[gnu::cold]] void fail (Error err) {
        output = Str(in_begin, in_end);
        scheme_end = path_end = query_end = 0;
        authority_end = u16(err);
    }
};

NOINLINE
IRI in::parse_and_canonicalize (Str ref, const IRI& base) noexcept {
    if (!ref) [[unlikely]] return IRI();
    IRIParser parser;
    parser.parse(ref, base);
    return IRI(
        move(parser.output),
        parser.scheme_end, parser.authority_end,
        parser.path_end, parser.query_end
    );
}

NOINLINE static
AnyString examine_hierarchical_path (const IRI& self, const IRI& base) {
    u32 tail = self.authority_end + 1;
    u32 i;
    for (
        i = self.authority_end + 1;
        i < self.path_end && i < base.path_end;
        i++
    ) {
        if (self.spec_[i] != base.spec_[i]) goto found_difference;
        else if (self.spec_[i] == '/') tail = i + 1;
    }
     // Okay, the paths are identical.  Return the last segment.
    if (tail != self.path_end) {
         // BUT if it contains : we need to prepend ./ so it isn't
         // interpreted as a scheme.
        for (usize i = tail; i < self.path_end; i++) {
            if (self.spec_[i] == '/') break;
            else if (self.spec_[i] == ':') {
                expect(self.spec_.size() < maximum_length);
                return cat("./", self.spec_.slice(tail));
            }
        }
        return expect(self.spec_.slice(tail));
    }
    else {
         // The identical paths end in /, what do we do?  Uh.....I think
         // we're supposed to return . then.
        return ".";
    }
    found_difference:
     // Okay the paths are different, so count how many extra segments are
     // in the base, and prepend that many ../s.
    u32 dotdots = 0;
    for (; i < base.path_end; i++) {
        if (base.spec_[i] == '/') dotdots++;
    }
    usize cap = dotdots * 3 + (self.spec_.size() - tail);
    expect(cap < UniqueString::max_size_);
    auto r = UniqueString(Capacity(cap));
    for (u32 i = 0; i < dotdots; i++) {
        r.append_expect_capacity("../");
    }
    r.append_expect_capacity(self.spec_.slice(tail));
    return r;
}

AnyString IRI::relative_to (const IRI& base) const noexcept {
    if (!*this) [[unlikely]] return "";
    else if (!base) {
        if (base.empty()) {
            return_everything:
            return spec_;
        }
        else [[unlikely]] return "";
    }
     // Instead of doing a string compare per component, find the
     // first place where the specs are different and then determine which
     // component that's in.
     // Can't return nothing, so pretend the last character is always different
    u32 s = spec_.size() - 1;
    u32 bs = base.spec_.size();
    if (bs < s) s = bs;
    u32 diff;
    expect(s > 0); // valid IRI is always at least two bytes
    for (diff = 0; diff < s; diff++) {
        if (spec_[diff] != base.spec_[diff]) break;
    }
     // We don't need to check for the existence of components, because if a
     // component doesn't exist its size will be 0, so there's no room for diff
     // to be inside of it (unless it's at the end, but the end won't be
     // selected because of subtracting 1 from the size above).
     //
     // We do need to make sure the base's component starts at the same place so
     // we don't accidentally think that foo:asdf#bar and foo:asdfqwer#bar are
     // the same before the #fragment.
    u32 tail;
    if (diff >= query_end && base.query_end == query_end) {
        tail = query_end;
        goto return_tail;
    }
    else if (diff >= path_end && base.path_end == path_end) {
        tail = path_end;
        goto return_tail;
    }
    else if (diff >= authority_end && base.authority_end == authority_end) {
        if (spec_[authority_end] != '/' ||
            base.spec_[base.authority_end] != '/'
        ) {
             // Non-hierchical paths can't be chopped any further.
            goto return_everything;
        }
         // TODO: reuse diff for this
        else return examine_hierarchical_path(*this, base);
    }
    else if (diff >= scheme_end && base.scheme_end == scheme_end) {
        tail = scheme_end + 1; // skip the :
        goto return_tail;
    }
    else goto return_everything;
    return_tail:
    return expect(spec_.slice(tail));
}

bool scheme_canonical (Str scheme) {
    const char* p = scheme.begin();
    const char* end = scheme.end();
    if (p >= end) return false;
    char c = *p++;
    if (c >= 'a' && c <= 'z') [[likely]] { }
    else return false;
    while (p < end) {
        char c = *p++;
        if (!char_scheme_valid(c)) [[unlikely]] return false;
        if (!char_scheme_canonical(c)) return false;
    }
    return true;
}

IRI IRI::add_slash_to_path () const {
    if (!hierarchical()) return IRI(Error::CouldNotResolve);
    if (spec_[path_end-1] == '/') return *this;
    else return IRI(
        cat(spec_.chop(path_end), '/', spec_.slice(path_end)),
        scheme_end, authority_end, path_end + 1, query_end + 1
    );
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
    {.i = "ayu-test:/#bar+1//bu%2Fp+33+0/3///", .s = "ayu-test", .p = "/", .f = "bar+1//bu%2Fp+33+0/3///"},
    {.i = "foo:/bar%25baz", .s = "foo", .p = "/bar%25baz"},
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
    for (u32 i = 0; i < n_cases; i++) {
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
            cases[i].i, " (", cases[i].b, ") error = ", u16(cases[i].e)
        ));
    }

     // Test compile-time IRI construction
    constexpr IRI c1 ("foo://bar/baz?qux#bap");
    is(c1.scheme(), "foo");
    is(c1.authority(), "bar");
    is(c1.path(), "/baz");
    is(c1.query(), "qux");
    is(c1.fragment(), "bap");
    //constexpr IRI invalid ("foo://bar/baz?qux#bap#bap");

    is(
        IRI("foo://bar/bup").relative_to(IRI("reb://bar/bup")),
        "foo://bar/bup",
        "relative_to with different scheme"
    );
    is(
        IRI("foo://bar/bup").relative_to(IRI("foo://bob/bup")),
        "//bar/bup",
        "relative_to with different authority"
    );
    is(
        IRI("foo:bar/bup").relative_to(IRI("foo:bar/bup")),
        "foo:bar/bup",
        "relative_to with non-heirarchical path"
    );
    is(
        IRI("foo:bar/bup?qal").relative_to(IRI("foo:bar/bup?qal")),
        "?qal",
        "relative_to with non-hierarchical path and query"
    );
    is(
        IRI("foo://bar/bup").relative_to(IRI("foo://bar/bup")),
        "bup",
        "relative_to with identical paths"
    );
    is(
        IRI("foo://bar/bup/").relative_to(IRI("foo://bar/bup/")),
        ".",
        "relative_to with identical paths with /"
    );
    is(
        IRI("foo://bar/bup:qal").relative_to(IRI("foo://bar/bup:qal")),
        "./bup:qal",
        "relative_to with in identical paths with :"
    );
    is(
        IRI("foo://bar/bup/gak?bee").relative_to(IRI("foo://bar/qal/por/bip")),
        "../../bup/gak?bee",
        "relative_to with ..s"
    );
    is(
        IRI("foo://bar/bup?qal").relative_to(IRI("foo://bar/bup?qal")),
        "?qal",
        "relative_to ending with query"
    );
    is(
        IRI("foo://bar/bup#qal").relative_to(IRI("foo://bar/bup#qal")),
        "#qal",
        "relative_to ending with fragment"
    );
    is(
        IRI("foo://bar/bup?qal#gak").relative_to(IRI("foo://bar/bup?qal#gak")),
        "#gak",
        "relative_to ending with query and fragment"
    );
    done_testing();
});

#endif
