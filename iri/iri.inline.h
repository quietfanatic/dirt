#pragma once

#include <array>

namespace iri {
namespace in {

enum CharProps : u8 {
    Ordinary = 0,  // Unreserved or subdelim
    Iffy = 1,  // These are allowed but are canonically encoded
    Forbidden = 2,  // These are forbidden if not encoded.
    Slash = 3,
    Question = 4,
    Hash = 5,
    Percent = 6,
    Behavior = 0x07,

    IsHexadecimal = 0x20,  // We have a spare bit, may as well use it
    SchemeValid = 0x40,
    WantsEncode = 0x80,  // Don't eagerly decode these
};

constexpr std::array<u8, 256> char_props = []{
    std::array<u8, 256> r = {};  // default Ordinary
    for (u8 c : {
        '<', '>', '"', '{', '}', '|', '\\', '^', '`'
    }) r[c] = CharProps::Iffy | CharProps::WantsEncode;
    for (u8 c : {
         0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
         0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
         0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x7f
    }) r[c] = CharProps::Forbidden | CharProps::WantsEncode;
    r['/'] = CharProps::Slash | CharProps::WantsEncode;
    r['?'] = CharProps::Question;
    r['#'] = CharProps::Hash;
    r['%'] = CharProps::Percent | CharProps::WantsEncode;

    for (u8 c : {
        ':', '/', '?', '#', '[', ']', '@', '!', '$', '&', '\'', '(', ')', '*',
        '+', ',', ';', '='
    }) r[c] |= CharProps::WantsEncode;

    for (u8 c = 'A'; c <= 'Z'; c++) r[c] |= CharProps::SchemeValid;
    for (u8 c = 'a'; c <= 'z'; c++) r[c] |= CharProps::SchemeValid;
    for (u8 c : {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '-', '.'
    }) r[c] |= CharProps::SchemeValid;
    for (u8 c : {
        '0','1','2','3','4','5','6','7','8','9',
        'a','b','c','d','e','f','A','B','C','D','E','F'
    }) r[c] |= CharProps::IsHexadecimal;
    return r;
}();

static constexpr
u8 char_behavior (char c) {
    return char_props[u8(c)] & CharProps::Behavior;
}
static constexpr
bool char_scheme_valid (char c) {
    return char_props[u8(c)] & CharProps::SchemeValid;
}
static constexpr
bool char_scheme_valid_start (char c) {
    return char_scheme_valid(c) && c & 0x40;
}
static constexpr
bool char_scheme_canonical (char c) {
    return c & 0x20;
}
static constexpr
bool char_wants_encode (char c) {
    return char_props[u8(c)] & CharProps::WantsEncode;
}
static constexpr
bool char_is_hexadecimal (char c) {
    return char_props[u8(c)] & CharProps::IsHexadecimal;
}

struct ConstexprValidator {
    const char* begin;
    const char* in;
    const char* end;
    u16 scheme_end;
    u16 authority_end;
    u16 path_end;
    u16 query_end;

    constexpr IRI parse (Str input, const IRI& base) {
        if (!base.empty()) ERROR_cannot_resolve_relative_IRIs_at_constexpr_time();
        if (!input) return IRI();
        if (input.size() > maximum_length) ERROR_input_too_long();
        begin = in = input.begin();
        end = input.end();
        parse_scheme();
        return IRI(
            StaticString(begin, end),
            scheme_end, authority_end, path_end, query_end
        );
    }

    constexpr void parse_scheme () {
        if (!char_scheme_valid_start(*in)) ERROR_invalid_scheme();
        if (!char_scheme_canonical(*in)) ERROR_canonical_scheme_must_be_lowercase();
        in++;
        while (in < end) {
            if (*in == ':') {
                scheme_end = in - begin;
                in++;
                if (in + 2 <= end && in[0] == '/' && in[1] == '/') {
                    return parse_authority();
                }
                else {
                    authority_end = in - begin;
                    if (in + 1 <= end && in[0] == '/') {
                        return parse_hierarchical_path();
                    }
                    else return parse_nonhierarchical_path();
                }
            }
            else if (char_scheme_valid(*in)) {
                if (!char_scheme_canonical(*in)) ERROR_canonical_scheme_must_be_lowercase();
                in++;
            }
            else ERROR_invalid_scheme();
        }
        ERROR_invalid_scheme();
    }

    constexpr void parse_authority() {
        in += 2;
        while (in < end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
                 // TODO: must be lowercase
                in++; break;
            case CharProps::Slash:
                authority_end = in - begin;
                return parse_hierarchical_path();
            case CharProps::Question:
                authority_end = path_end = in - begin;
                return parse_query();
            case CharProps::Hash:
                authority_end = path_end = query_end = in - begin;
                return parse_fragment();
            case CharProps::Percent:
                validate_percent(in, end);
                in += 3; break;
            case CharProps::Iffy:
                ERROR_character_must_canonically_be_percent_encoded();
            case CharProps::Forbidden:
                ERROR_invalid_authority();
            default: never();
        }
        authority_end = path_end = query_end = in - begin;
        return;
    }

    constexpr void parse_hierarchical_path () {
        in++;
        while (in < end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
                in++; break;
            case CharProps::Slash:
                validate_segment(in);
                in++; break;
            case CharProps::Question:
                validate_segment(in);
                path_end = in - begin;
                return parse_query();
            case CharProps::Hash:
                validate_segment(in);
                path_end = query_end = in - begin;
                return parse_fragment();
            case CharProps::Percent:
                validate_percent(in, end);
                in += 3; break;
            case CharProps::Iffy:
                ERROR_character_must_canonically_be_percent_encoded();
            case CharProps::Forbidden:
                ERROR_invalid_path();
            default: never();
        }
        validate_segment(in);
        path_end = query_end = in - begin;
        return;
    }

    constexpr void parse_nonhierarchical_path () {
        while (in < end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
            case CharProps::Slash:
                in++; break;
            case CharProps::Question:
                path_end = in - begin;
                return parse_query();
            case CharProps::Hash:
                path_end = query_end = in - begin;
                return parse_fragment();
            case CharProps::Percent:
                validate_percent(in, end);
                in += 3; break;
            case CharProps::Iffy:
                ERROR_character_must_canonically_be_percent_encoded();
            case CharProps::Forbidden:
                ERROR_invalid_path();
            default: never();
        }
        path_end = query_end = in - begin;
        return;
    }

    constexpr void parse_query () {
        in++;
        while (in < end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
            case CharProps::Slash:
            case CharProps::Question:
                in++; break;
            case CharProps::Hash:
                query_end = in - begin;
                return parse_fragment();
            case CharProps::Percent:
                validate_percent(in, end);
                in += 3; break;
            case CharProps::Iffy:
                ERROR_character_must_canonically_be_percent_encoded();
            case CharProps::Forbidden:
                ERROR_invalid_query();
            default: never();
        }
        query_end = in - begin;
        return;
    }

    constexpr void parse_fragment () {
        in++;
        while (in < end) switch (char_behavior(*in)) {
            case CharProps::Ordinary:
            case CharProps::Slash:
            case CharProps::Question:
                in++; break;
            case CharProps::Percent:
                validate_percent(in, end);
                in += 3; break;
            case CharProps::Iffy:
                ERROR_character_must_canonically_be_percent_encoded();
            case CharProps::Forbidden:
            case CharProps::Hash:
                ERROR_invalid_fragment();
            default: never();
        }
        return;
    }

    static constexpr void validate_percent (const char* in, const char* end) {
        if (in + 3 > end) ERROR_invalid_percent_sequence();
        u8 byte = 0;
        for (int i = 1; i < 3; i++) {
            byte <<= 4;
            char c = in[i];
            if (c >= '0' && c <= '9') byte |= c - '0';
            else if (c >= 'A' && c <= 'F') byte |= c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') {
                ERROR_canonical_percent_sequence_must_be_uppercase();
            }
            else ERROR_invalid_percent_sequence();
        }
        if (char_wants_encode(byte)) return;
        else ERROR_character_must_canonically_not_be_percent_encoded();
    }

    static constexpr void validate_segment (const char* in) {
         // The IRI is at least 4 chars so far (like "a://")
        if (Str(in-3, 3) == "/.." || Str(in-2, 2) == "/.") {
            ERROR_canonical_path_cannot_have_dot_or_dotdot();
        }
    }

     // Still the best (least worst) way to make compile-time error messages.
    [[noreturn]] static void ERROR_cannot_resolve_relative_IRIs_at_constexpr_time () { never(); }
    [[noreturn]] static void ERROR_input_too_long () { never(); }
    [[noreturn]] static void ERROR_invalid_scheme () { never(); }
    [[noreturn]] static void ERROR_invalid_authority () { never(); }
    [[noreturn]] static void ERROR_invalid_path () { never(); }
    [[noreturn]] static void ERROR_invalid_query () { never(); }
    [[noreturn]] static void ERROR_invalid_fragment () { never(); }
    [[noreturn]] static void ERROR_invalid_percent_sequence () { never(); }
    [[noreturn]] static void ERROR_character_must_canonically_be_percent_encoded () { never(); }
    [[noreturn]] static void ERROR_character_must_canonically_not_be_percent_encoded () { never(); }
    [[noreturn]] static void ERROR_canonical_percent_sequence_must_be_uppercase () { never(); }
    [[noreturn]] static void ERROR_canonical_scheme_must_be_lowercase () { never(); }
    [[noreturn]] static void ERROR_canonical_path_cannot_have_dot_or_dotdot () { never(); }
};

IRI parse_and_canonicalize (Str ref, const IRI& base) noexcept;

} // in

 // This is very branchy but abusing CharProps doesn't make it any better.
constexpr Relativity relativity (Str ref) {
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
    for (char c : ref.slice(1)) {
        switch (c) {
            case ':': return Relativity::Scheme;
            case '/': case '?': case '#': return Relativity::RelativePath;
            default: break;
        }
    }
    return Relativity::RelativePath;
}

constexpr IRI::IRI (Str ref, const IRI& base) :
    IRI(std::is_constant_evaluated()
        ? in::ConstexprValidator().parse(ref, base)
        : in::parse_and_canonicalize(ref, base)
    )
{ }

constexpr IRI::IRI (AnyString spec, u16 c, u16 p, u16 q, u16 h) :
    spec_(move(spec)), scheme_end(c), authority_end(p), path_end(q), query_end(h)
{ }

constexpr IRI::IRI (Error code, const AnyString& spec) :
    spec_(spec), authority_end(u16(code))
{ expect(code != Error::NoError && code != Error::Empty); }

constexpr IRI::IRI (const IRI& o) = default;
constexpr IRI::IRI (IRI&& o) :
    spec_(move(const_cast<AnyString&>(o.spec_))),
    scheme_end(o.scheme_end),
    authority_end(o.authority_end),
    path_end(o.path_end),
    query_end(o.query_end)
{
    if (!std::is_constant_evaluated()) {
        const_cast<u16&>(o.scheme_end) = 0;
        const_cast<u16&>(o.authority_end) = 0;
        const_cast<u16&>(o.path_end) = 0;
        const_cast<u16&>(o.query_end) = 0;
    }
}
constexpr IRI& IRI::operator = (const IRI& o) {
    if (this == &o) return *this;
    this->~IRI();
    new (this) IRI(o);
    return *this;
}
constexpr IRI& IRI::operator = (IRI&& o) {
    if (this == &o) return *this;
    this->~IRI();
    new (this) IRI(move(o));
    return *this;
}

constexpr bool IRI::valid () const { return scheme_end; }
constexpr bool IRI::empty () const { return !scheme_end && !authority_end; }
constexpr IRI::operator bool () const { return scheme_end; }
constexpr Error IRI::error () const {
    if (scheme_end) return Error::NoError;
    else if (spec_.empty()) return Error::Empty;
    else return Error(authority_end);
}

static constexpr const AnyString empty_string = StaticString();

constexpr const AnyString& IRI::spec () const {
    if (scheme_end) return spec_;
    else return empty_string;
}
constexpr const AnyString& IRI::possibly_invalid_spec () const {
    return spec_;
}

constexpr AnyString IRI::move_spec () {
    if (!scheme_end) [[unlikely]] return "";
    AnyString r = move(spec_);
    *this = IRI();
    return r;
}
constexpr AnyString IRI::move_possibly_invalid_spec () {
    AnyString r = move(spec_);
    *this = IRI();
    return r;
}

constexpr bool IRI::has_scheme () const { return scheme_end; }
constexpr bool IRI::has_authority () const { return scheme_end && authority_end >= scheme_end + 3; }
constexpr bool IRI::has_path () const { return scheme_end && path_end > authority_end; }
constexpr bool IRI::has_query () const { return scheme_end && query_end > path_end; }
constexpr bool IRI::has_fragment () const { return scheme_end && spec_.size() > query_end; }

constexpr bool IRI::hierarchical () const {
    return has_authority() || (has_path() && spec_[authority_end] == '/');
}
constexpr bool IRI::nonhierarchical () const {
    return has_path() && spec_[authority_end] != '/';
}

constexpr Str IRI::scheme () const {
    if (!has_scheme()) return "";
    return spec_.slice(0, scheme_end);
}
constexpr Str IRI::authority () const {
    if (!has_authority()) return "";
    return spec_.slice(scheme_end + 3, authority_end);
}
constexpr Str IRI::path () const {
    if (!has_path()) return "";
    return spec_.slice(authority_end, path_end);
}
constexpr Str IRI::query () const {
    if (!has_query()) return "";
    return spec_.slice(path_end + 1, query_end);
}
constexpr Str IRI::fragment () const {
    if (!has_fragment()) return "";
    return spec_.slice(query_end + 1, spec_.size());
}

constexpr IRI IRI::chop_authority () const {
    if (!scheme_end) [[unlikely]] return IRI(Error::InputInvalid);
    return IRI(
        spec_.chop(scheme_end+1),
        scheme_end, scheme_end+1, scheme_end+1, scheme_end+1
    );
}
constexpr IRI IRI::chop_path () const {
    if (!scheme_end) [[unlikely]] return IRI(Error::InputInvalid);
    return IRI(
        spec_.chop(authority_end),
        scheme_end, authority_end, authority_end, authority_end
    );
}
constexpr IRI IRI::chop_filename () const {
    if (!scheme_end) [[unlikely]] return IRI(Error::InputInvalid);
    if (!hierarchical()) [[unlikely]] return IRI(Error::CouldNotResolve);
    u32 i = path_end;
    while (spec_[i-1] != '/') --i;
    return IRI(
        spec_.chop(i),
        scheme_end, authority_end, i, i
    );
}
constexpr IRI IRI::chop_last_slash () const {
    if (!scheme_end) [[unlikely]] return IRI(Error::InputInvalid);
    if (!hierarchical()) [[unlikely]] return IRI(Error::CouldNotResolve);
    u32 i = path_end;
    while (spec_[i-1] != '/') --i;
    --i;
    if (i == authority_end) [[unlikely]] return IRI(Error::PathOutsideRoot);
    return IRI(
        spec_.chop(i),
        scheme_end, authority_end, i, i
    );
}
constexpr IRI IRI::chop_query () const {
    if (!scheme_end) [[unlikely]] return IRI(Error::InputInvalid);
    return IRI(
        spec_.chop(path_end),
        scheme_end, authority_end, path_end, path_end
    );
}
constexpr IRI IRI::chop_fragment () const {
    if (!scheme_end) [[unlikely]] return IRI(Error::InputInvalid);
    return IRI(
        spec_.chop(query_end),
        scheme_end, authority_end, path_end, query_end
    );
}

constexpr IRI IRI::chop (usize new_size) const {
    if (!scheme_end) [[unlikely]] return IRI(Error::InputInvalid);
    if (new_size > spec_.size()) [[unlikely]] return *this;
    if (new_size <= scheme_end) [[unlikely]] {
        return IRI(Error::SchemeInvalid, spec_.chop(new_size));
    }
    if (spec_[new_size-1] == '%' || spec_[new_size-2] == '%') [[unlikely]] {
        return IRI(Error::PercentSequenceInvalid, spec_.chop(new_size));
    }
    if (has_authority() && new_size == usize(scheme_end) + 2) {
         // Tried to chop between the //s introducing the authority.  Not sure
         // what error code to return in this case, since the resulting IRI
         // isn't actually invalid, we're just rejecting it because its meaning
         // has changed in an unintended way.
        return IRI(Error::InputInvalid, spec_.chop(new_size));
    }
    u16 a = authority_end;
    u16 p = path_end;
    u16 q = query_end;
    if (new_size < authority_end) {
        a = p = q = new_size;
    }
    else if (new_size < path_end) {
        p = q = new_size;
    }
    else if (new_size < query_end) {
        q = new_size;
    }
    return IRI(
        spec_.chop(new_size), scheme_end, a, p, q
    );
}
constexpr IRI IRI::chop (const char* new_end) const {
    expect(new_end >= spec_.begin() && new_end <= spec_.end());
    return chop(new_end - spec_.begin());
}

constexpr IRI::~IRI () { }

} // iri
