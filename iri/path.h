#pragma once

#include "iri.h"
#include "../uni/arrays.h"
#include "../uni/common.h"

 // Various utilities for manipulating the path portion of IRIs.  In a pinch
 // these can be used to process OS paths too.  They don't do any validation or
 // canonicalization, so using them on malformed paths is not guaranteed to give
 // sensible results.

namespace iri {
using namespace uni;

 // Like encode(), but only percent-encodes ?, #, and %.
UniqueString encode_path (Str) noexcept;

 // Return the path without the filename at the end.  The return value always
 // ends in a slash.
 //    path_without_filename("foo/bar") == "foo/"
 //    path_without_filename("foo/") == "foo/"
 //    path_without_filename("foo") == "./"
Str path_without_filename (Str) noexcept;

 // Same as above, but without the trailing slash.
 //    path_parent("foo/bar") == "foo"
 //    path_parent("foo/") == "foo"
 //    path_parent("foo") == "."
Str path_parent (Str) noexcept;

 // Get the filename extension, if any; that is, everything after the last dot
 // in the last segment of the path, or empty if there is no dot.  Does not
 // include the dot.
Str path_extension (Str) noexcept;

 // The IRI representing the root of the filesystem.
constexpr IRI file_root ("file:///");

 // Create an IRI from an OS filesystem path.  Will be converted to absolute
 // form, then appended to file_prefix.
IRI iri_from_fs_path (Str, const IRI& base = IRI()) noexcept;

 // Get a path from the given IRI.  The IRI must start with file:// and must not
 // have a query or fragment (to pull them off first, use iri.without_query()).
UniqueString iri_to_fs_path (const IRI&) noexcept;

} // iri
