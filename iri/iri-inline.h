#pragma once

namespace iri {

constexpr IRI::IRI (AnyString spec, uint16 c, uint16 p, uint16 q, uint16 h) :
    spec_(move(spec)), scheme_end(c), authority_end(p), path_end(q), query_end(h)
{ }

constexpr IRI::IRI (const IRI& o) = default;
constexpr IRI::IRI (IRI&& o) :
    spec_(move(const_cast<AnyString&>(o.spec_))),
    scheme_end(o.scheme_end),
    authority_end(o.authority_end),
    path_end(o.path_end),
    query_end(o.query_end)
{ new (&o) IRI(); }
constexpr IRI& IRI::operator = (const IRI& o) {
    if (this == &o) return *this;;
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
constexpr bool IRI::empty () const { return spec_.empty(); }
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
    if (!scheme_end) return "";
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
    return has_path() && spec_[authority_end] == '/';
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

constexpr IRI IRI::with_scheme_only () const {
    if (!scheme_end) return IRI();
    return IRI(
        spec_.shrunk(scheme_end+1),
        scheme_end, scheme_end+1, scheme_end+1, scheme_end+1
    );
}
constexpr IRI IRI::with_origin_only () const {
    if (!scheme_end) return IRI();
    return IRI(
        spec_.shrunk(authority_end),
        scheme_end, authority_end, authority_end, authority_end
    );
}
constexpr IRI IRI::without_filename () const {
    if (!hierarchical()) return IRI();
    uint32 i = path_end;
    while (spec_[i-1] != '/') --i;
    return IRI(
        spec_.shrunk(i),
        scheme_end, authority_end, i, i
    );
}
constexpr IRI IRI::without_query () const {
    if (!scheme_end) return IRI();
    return IRI(
        spec_.shrunk(path_end),
        scheme_end, authority_end, path_end, path_end
    );
}
constexpr IRI IRI::without_fragment () const {
    if (!scheme_end) return IRI();
    return IRI(
        spec_.shrunk(query_end),
        scheme_end, authority_end, path_end, query_end
    );
}

constexpr Str IRI::spec_with_scheme_only () const {
    if (!scheme_end) return "";
    return spec_.slice(0, scheme_end + 1);
}
constexpr Str IRI::spec_with_origin_only () const {
    if (!scheme_end) return "";
    return spec_.slice(0, authority_end);
}
constexpr Str IRI::spec_without_filename () const {
    if (!hierarchical()) return "";
    uint32 i = path_end;
    while (spec_[i-1] != '/') --i;
    return spec_.slice(0, i);
}
constexpr Str IRI::spec_without_query () const {
    if (!scheme_end) return "";
    return spec_.slice(0, path_end);
}
constexpr Str IRI::spec_without_fragment () const {
    if (!scheme_end) return "";
    return spec_.slice(0, query_end);
}

constexpr Str IRI::path_without_filename () const {
    if (!hierarchical()) return "";
    uint32 i = path_end;
    while (spec_[i-1] != '/') --i;
    return spec_.slice(authority_end, i);
}

constexpr IRI::~IRI () { }

} // iri
