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

///// PATH MANIPULATION

 // Converts a filesystem path into a string appropriate for use in an IRI path.
 // %-encodes characters that can't be in a path (a subset of those encoded by
 // encode()), and on systems with \s a separators, converts \s to /s.
UniqueString encode_path (Str) noexcept;
 // Just an alias for decode().  Does NOT convert /s back to \s.
 // TODO: should it?
inline UniqueString decode_path (Str s) { return iri::decode(s); }

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
 // include the dot.  TODO: return empty for files whose only dot is the first
 // character.
Str path_extension (Str) noexcept;

///// FILE SCHEME IRIS

constexpr IRI file_scheme ("file:");

 // Get a file: IRI corresponding to the working directory the program was run
 // from.  WARNING: this is cached and only generated once.  You shouldn't be
 // chdiring in the middle of a program anyway.
const IRI& working_directory () noexcept;

 // Get a file: IRI corresponding to the location of the currently running
 // program.  This is also only generated once, but if you somehow manage to
 // change the location of the program while running it, you deserve whatever
 // happens.
 //
 // Use program_location().without_filename() to get the directory containing
 // the program.
const IRI& program_location () noexcept;

///// TO/FROM FILESYSTEM PATHS

 // Create an IRI from an OS filesystem path.  Will be converted to absolute
 // form, then appended to file_scheme.  The (empty) authority will be omitted,
 // meaning file:/foo/bar, not file:///foo/bar.  If base is not provided,
 // relative paths will be resolved against working_directory().
IRI from_fs_path (Str, const IRI& base = IRI()) noexcept;

 // Get a path from the given IRI.  The IRI must start with file:/ and must not
 // have an non-empty authority or a query or fragment.
UniqueString to_fs_path (const IRI&) noexcept;

} // iri
