// A library for IRIs (Internationalized Resource Identifiers).
// Basically URIs but unicode.
// Under heavy development!  Don't use for anything important.
//
// Requires C++17 or later.
//
///// IRI HANDLING, POSSIBLE DEVIATIONS FROM SPECIFICATIONS
//
// This library is scheme-agnostic.  Parsing is the same for all schemes, so if
// there's a scheme that uses non-standard syntax it may not work properly.
//
// The authority (hostname or IP address, port, possible username) is opaque to
// this library.  It might let through some invalid authority components.
//
// Unlike most URI parsing libraries, this will leave non-ascii UTF-8 as-is,
// without %-encoding it, which is what makes an IRI.  This library does not
// validate UTF-8 sequences.  If invalid UTF-8 is given, it will be passed
// through.
//
// Uppercase ASCII in the scheme and authority will be canonicalized to
// lowercase.  Non-ASCII is NOT canonicalized to lowercase in the authority
// (and it's forbidden in the scheme).
//
// ASCII Whitespace is rejected as invalid in all cases.  This may differ from
// other URI libraries, which may accept whitespace for some schemes such as
// data:.  Non-ASCII whitespace is passed through, since detecting it would
// require importing unicode tables, which are very large.
//
// IRIs with a path that starts with /.. will be rejected, unlike with most URI
// libraries, which will silently drop the .. segment.
//
// IRIs in this library cannot be longer than 65535 bytes.
//
// Since this is a very new and undertested library, there are likely to be some
// errors in handling IRIs.  If the behavior differs from the specifications:
//     https://datatracker.ietf.org/doc/html/rfc3987 - IRI
//     https://datatracker.ietf.org/doc/html/rfc3986 - URI
// then it is this library that is incorrect.
//
///// Interface
//
// None of the strings returned by any methods will be NUL-terminated.
//
// Will not throw when given an invalid IRI spec.  Instead will mark the IRI as
// invalid, and all accessors will return false or empty.  You can see what went
// wrong by looking at the return of possibly_invalid_spec().
//
// The component getter functions will not decode % sequences, because which
// characters have to be % encoding can be application-specific.  Call decode()
// yourself on the results when you want to decode them.
//
// The IRI class is pretty lightweight, with one reference-counted string and
// four uint16s.  16 bytes on 32-bit and 24 bytes on 64-bit.  However it is NOT
// threadsafe.  If you want to pass IRIs between threads, martial them through
// UniqueString first.
//
// There are no facilities for parsing query strings yet.
//
// Not all functions of IRI are constexpr.  In particular, parsing from a string
// is not constexpr.

#pragma once

#include "../uni/common.h"
#include "../uni/strings.h"

namespace iri {
using namespace uni;

constexpr uint32 maximum_length = uint16(-1);

 // Replace reserved characters with % sequences
UniqueString encode (Str) noexcept;
 // Replace % sequences with their characters.  If there's an invalid escape
 // sequence anywhere in the input, returns the empty string.
UniqueString decode (Str) noexcept;

 // The first component that the given IRI reference has.
enum class Relativity {
    Scheme,       // scheme://auth/path?query#fragment
    Authority,    // //auth/path?query#fragment
    AbsolutePath, // /path?query#fragment
    RelativePath, // path?query#fragment
    Query,        // ?query#fragment
    Fragment      // #fragment
};

 // Return what kind of relative reference this is.  This only does basic
 // detection, and when given an invalid reference, may return anything.  To be
 // sure that the reference is valid, resolve it into a full IRI.
constexpr Relativity relativity (Str);

 // What went wrong when parsing an IRI
enum class Error : uint16 {
     // This IRI is not actually invalid.
    NoError,
     // This IRI is empty.
    Empty,
     // This IRI is longer than 64k.
    TooLong,
     // Was unable to resolve a relative IRI reference, because the base was
     // empty or invalid, or because the IRI reference was AbsolutePath or
     // RelativePath, but the base was nonhierarchical.
    CouldNotResolve,
     // The given component is invalid (contains invalid characters or ends in
     // whitespace).
    SchemeInvalid,
    AuthorityInvalid,
    PathInvalid,
    QueryInvalid,
    FragmentInvalid,
     // The path had too many .. segments.  This is a deviation from the URI
     // specs, which say that http://example.com/../foo should be canonicalized
     // into http://example.com/foo.  This library errors instead.
    PathOutsideRoot,
     // There's a % that isn't followed by two hexadecimal digits.
    PercentSequenceInvalid,
};

struct IRI {
     // Construct the empty IRI.  This is not a valid IRI.
    constexpr IRI () { }
     // Construct from an IRI string.  Does validation and canonicalization.  If
     // base is provided, resolved ref as a IRI reference (AKA a relative IRI)
     // with base as its base. If base is not provided, ref must be an absolute
     // IRI with scheme included.
     //
     // The behavior of this function changes when run at constexpr time.  It
     // cannot canonicalize the IRI, because new strings can't be allocated at
     // compile time (and kept for run time).  So it must be given an IRI that
     // is already fully resolved and canonical.  The base will be ignored.
    constexpr explicit IRI (Str ref, const IRI& base = IRI());

     // Construct an already-parsed IRI.  This will not do any validation.  If
     // you provide invalid parameters, you will wreak havoc and mayhem.
    constexpr explicit IRI (
        AnyString spec,
        uint16 scheme_end, uint16 authority_end,
        uint16 path_end, uint16 query_end
    );

     // Copy and move construction and assignment
    constexpr IRI (const IRI& o);
    constexpr IRI (IRI&& o);
    constexpr IRI& operator = (const IRI& o);
    constexpr IRI& operator = (IRI&& o);

     // Returns whether this IRI is valid or not.  If the IRI is invalid, all
     // bool accessors will return false and all string and IRI accessors will
     // return empty.
    constexpr bool valid () const;
     // Returns whether this IRI is empty.  The empty IRI is also invalid, but
     // not all invalid IRIs are empty.
    constexpr bool empty () const;
     // Equivalent to valid()
    explicit constexpr operator bool () const;

     // Check what's wrong with this IRI
    constexpr Error error () const;

     // Gets the full text of the IRI only if this IRI is valid.
    constexpr const AnyString& spec () const;
     // Get full text of IRI even it is not valid.  This is only for diagnosing
     // what is wrong with the IRI.  Don't use it for anything important.
    constexpr const AnyString& possibly_invalid_spec () const;

     // Steal the spec string, leaving this IRI empty.
    constexpr AnyString move_spec ();
     // Steal the spec string even if it's invalid.
    constexpr AnyString move_possibly_invalid_spec ();

     // Check for existence of components.
    constexpr bool has_scheme () const;
    constexpr bool has_authority () const;
    constexpr bool has_path () const;
    constexpr bool has_query () const;
    constexpr bool has_fragment () const;

     // If there is an authority or a path that starts with /.
    constexpr bool hierarchical () const;
     // If there is a path and it doesn't start with /.  This is almost the
     // opposite of the above, but both will return false for an IRI that is
     // just a scheme.
    constexpr bool nonhierarchical () const;

     // Get the scheme of the IRI.  Doesn't include the :.
     // This will always return something for a valid IRI.
    constexpr Str scheme () const;
     // Get the authority (host and port).  Doesn't include the //.  Will
     // return empty if has_authority is false.  May still return empty if
     // has_authority is true, but the IRI has an empty authority (e.g.
     // file:///foo/bar)
    constexpr Str authority () const;
     // Get the path component of the IRI.
     //   scheme://host/path -> /path
     //   scheme://host/ -> /
     //   scheme://host -> (empty, has_path will be false)
     //   scheme:///path -> /path
     //   scheme:/path -> /path
     //   scheme:path -> path
     // If has_path is true, will always return non-empty.
    constexpr Str path () const;
     // Get the query.  Will not include the ?.  May be existent but empty.
    constexpr Str query () const;
     // Get the fragment.  Will not include the #.  May be existent but empty.
    constexpr Str fragment () const;

     // Returns a new IRI with just the scheme (and the colon).
    constexpr IRI with_scheme_only () const;
     // Get the origin (scheme plus authority if it exists).  Never ends with
     // a / (unless the authority exists and is empty, like foo://).
    constexpr IRI with_origin_only () const;
     // Get everything up to and including the last / in the path.  If the path
     // ends in /, returns the same IRI (but without the query or fragment).
     // If the IRI is not hierarchical (path doesn't start with /), returns an
     // invalid IRI with error() == Error::CouldNotResolve.
    constexpr IRI without_filename () const;
     // Like without_filename but also takes off the last /.  If the path ends
     // with /, just the / will be taken off.  If the path is the root (contains
     // only a /), returns an invalid IRI with error() ==
     // Error::PathOutsideRoot.
    constexpr IRI without_last_segment () const;
     // Get the scheme, authority, and path but not the query or fragment.
    constexpr IRI without_query () const;
     // Get everything but the fragment
    constexpr IRI without_fragment () const;

     // The following are the same as above, but return a raw Str instead of a
     // new IRI.  This saves a refcount, but can cost an extra parse if you turn
     // the Str back into an IRI.
    constexpr Str spec_with_scheme_only () const;
    constexpr Str spec_with_origin_only () const;
    constexpr Str spec_without_filename () const;
    constexpr Str spec_without_last_segment () const;
    constexpr Str spec_without_query () const;
    constexpr Str spec_without_fragment () const;

     // Get an IRI reference that's relative to base, such that
     //     IRI(input.relative_to(base), base) == input
     // If the base IRI is empty, returns input.spec() unchanged (to preserve
     // the above equation).  If the base IRI is any other invalid IRI or the
     // input IRI is invalid (including empty), returns the empty string.
     // Otherwise never returns empty.
    AnyString relative_to (const IRI& base) const noexcept;

     // Destruct this object
    constexpr ~IRI ();

     // Comparisons just do string comparisons on the spec
#define IRI_FRIEND_OP(op) \
    friend constexpr auto operator op (const IRI& a, const IRI& b) { \
        return a.spec_ op b.spec_; \
    } \
    friend constexpr auto operator op (const IRI& a, Str b) { \
        return a.spec_ op b; \
    } \
    friend constexpr auto operator op (Str a, const IRI& b) { \
        return a op b.spec_; \
    }
#if __cplusplus >= 202002L
    IRI_FRIEND_OP(<=>)
    IRI_FRIEND_OP(==)
#else
    IRI_FRIEND_OP(<)
    IRI_FRIEND_OP(<=)
    IRI_FRIEND_OP(==)
    IRI_FRIEND_OP(!=)
    IRI_FRIEND_OP(>=)
    IRI_FRIEND_OP(>)
#endif
#undef IRI_FRIEND_OP

    const AnyString spec_;
    const uint16 scheme_end = 0;
    const uint16 authority_end = 0;
    const uint16 path_end = 0;
    const uint16 query_end = 0; // reused to store error
};

} // namespace iri

 // Implement std::hash so you can use IRIs in unordered maps
template <>
struct std::hash<iri::IRI> {
    std::size_t operator() (const iri::IRI& x) const {
        return std::hash<uni::arrays::AnyString>{}(x.spec_);
    }
};

#include "iri.inline.h"
