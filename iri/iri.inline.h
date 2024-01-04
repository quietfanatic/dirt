#pragma once

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

namespace iri {

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

namespace in {

struct ConstexprValidator {
    const char* begin;
    const char* in;
    const char* end;
    uint16 scheme_end;
    uint16 authority_end;
    uint16 path_end;
    uint16 query_end;

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
        switch (*in) {
            case IRI_LOWERCASE: in++; break;
            default: ERROR_invalid_scheme();
        }
        while (in < end) switch (*in) {
            case IRI_LOWERCASE: case IRI_DIGIT: case '+': case '-': case '.':
                in++; break;
            case ':':
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
            default: ERROR_invalid_scheme();
        }
        ERROR_invalid_scheme();
    }

    constexpr void parse_authority() {
        in += 2;
        while (in < end) switch (*in) {
            case IRI_UPPERCASE: case IRI_LOWERCASE: case IRI_DIGIT:
            case IRI_UNRESERVED_SYMBOL: case IRI_UTF8_HIGH: case IRI_SUBDELIM:
            case ':': case '[': case ']': case '@':
                in++; break;
            case '/':
                authority_end = in - begin;
                return parse_hierarchical_path();
            case '?':
                authority_end = path_end = in - begin;
                return parse_query();
            case '#':
                authority_end = path_end = query_end = in - begin;
                return parse_fragment();
            case '%':
                validate_percent(in, end);
                in += 3; break;
            case IRI_IFFY: ERROR_character_must_canonically_be_percent_encoded();
            default: ERROR_invalid_authority();
        }
        authority_end = path_end = query_end = in - begin;
        return;
    }

    constexpr void parse_hierarchical_path () {
        in++;
        while (in < end) switch (*in) {
            case IRI_UPPERCASE: case IRI_LOWERCASE:
            case IRI_DIGIT: case IRI_SUBDELIM:
            case IRI_UTF8_HIGH:
            case '-': case '_': case '~': case ':': case '@': case '.':
                in++; break;
            case '/':
                validate_segment(in);
                in++; break;
            case '?':
                validate_segment(in);
                path_end = in - begin;
                return parse_query();
            case '#':
                validate_segment(in);
                path_end = query_end = in - begin;
                return parse_fragment();
            case '%':
                validate_percent(in, end);
                in += 3; break;
            case IRI_IFFY: ERROR_character_must_canonically_be_percent_encoded();
            default: ERROR_invalid_path();
        }
        validate_segment(in);
        path_end = query_end = in - begin;
        return;
    }

    constexpr void parse_nonhierarchical_path () {
        while (in < end) switch (*in) {
            case IRI_UPPERCASE: case IRI_LOWERCASE: case IRI_DIGIT:
            case IRI_SUBDELIM: case IRI_UTF8_HIGH:
            case '-': case '_': case '~': case ':': case '@': case '/':
                in++; break;
            case '?':
                path_end = in - begin;
                return parse_query();
            case '#':
                path_end = query_end = in - begin;
                return parse_fragment();
            case '%':
                validate_percent(in, end);
                in += 3; break;
            case IRI_IFFY: ERROR_character_must_canonically_be_percent_encoded();
            default: ERROR_invalid_path();
        }
        path_end = query_end = in - begin;
        return;
    }

    constexpr void parse_query () {
        in++;
        while (in < end) switch (*in) {
            case IRI_UNRESERVED: case IRI_SUBDELIM:
            case ':': case '@': case '/': case '?':
                in++; break;
            case '#':
                query_end = in - begin;
                return parse_fragment();
            case '%':
                validate_percent(in, end);
                in += 3; break;
            case IRI_IFFY: ERROR_character_must_canonically_be_percent_encoded();
            default: ERROR_invalid_query();
        }
        query_end = in - begin;
        return;
    }

    constexpr void parse_fragment () {
        in++;
        while (in < end) switch (*in) {
            case IRI_UNRESERVED: case IRI_SUBDELIM:
            case ':': case '@': case '/': case '?':
                in++;
                break;
            case '%':
                validate_percent(in, end);
                in += 3; break;
            case IRI_IFFY: ERROR_character_must_canonically_be_percent_encoded();
            default: ERROR_invalid_fragment();
        }
        return;
    }

    static constexpr void validate_percent (const char* in, const char* end) {
        if (in + 3 > end) ERROR_invalid_percent_sequence();
        uint8 byte = 0;
        for (int i = 1; i < 3; i++) {
            byte <<= 4;
            switch (in[i]) {
                case IRI_DIGIT: byte |= in[i] - '0'; break;
                case IRI_UPPERHEX: byte |= in[i] - 'A' + 10; break;
                case IRI_LOWERHEX: ERROR_canonical_percent_sequence_must_be_uppercase();
                default: ERROR_invalid_percent_sequence();
            }
        }
        switch (char(byte)) {
            case IRI_GENDELIM: case IRI_SUBDELIM:
            case IRI_FORBIDDEN: case IRI_IFFY: return;
            default: ERROR_character_must_canonically_not_be_percent_encoded();
        }
    }

    static constexpr void validate_segment (const char* in) {
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
    [[noreturn]] static void ERROR_canonical_path_cannot_have_dot_or_dotdot () { never(); }
};

IRI parse_and_canonicalize (Str ref, const IRI& base) noexcept;

} // in

constexpr IRI::IRI (Str ref, const IRI& base) :
    IRI(std::is_constant_evaluated()
        ? in::ConstexprValidator().parse(ref, base)
        : in::parse_and_canonicalize(ref, base)
    )
{ }

constexpr IRI::IRI (AnyString spec, uint16 c, uint16 p, uint16 q, uint16 h) :
    spec_(move(spec)), scheme_end(c), authority_end(p), path_end(q), query_end(h)
{ }

constexpr IRI::IRI (Error code, const AnyString& spec) :
    spec_(spec), query_end(uint16(code))
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
        const_cast<uint16&>(o.scheme_end) = 0;
        const_cast<uint16&>(o.authority_end) = 0;
        const_cast<uint16&>(o.path_end) = 0;
        const_cast<uint16&>(o.query_end) = 0;
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
constexpr bool IRI::empty () const { return !scheme_end && !query_end; }
constexpr IRI::operator bool () const { return scheme_end; }
constexpr Error IRI::error () const {
    if (scheme_end) return Error::NoError;
    else if (spec_.empty()) return Error::Empty;
    else return Error(query_end);
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
constexpr bool IRI::has_authority () const { return authority_end >= scheme_end + 3; }
constexpr bool IRI::has_path () const { return path_end > authority_end; }
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
    uint32 i = path_end;
    while (spec_[i-1] != '/') --i;
    return IRI(
        spec_.chop(i),
        scheme_end, authority_end, i, i
    );
}
constexpr IRI IRI::chop_last_slash () const {
    if (!scheme_end) [[unlikely]] return IRI(Error::InputInvalid);
    if (!hierarchical()) [[unlikely]] return IRI(Error::CouldNotResolve);
    uint32 i = path_end;
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
    uint16 a = authority_end;
    uint16 p = path_end;
    uint16 q = query_end;
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
