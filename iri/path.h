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
 // encode()), and on systems with \s as separators, converts \s to /s.
UniqueString encode_path (Str) noexcept;
 // Just an alias for decode().  Does NOT convert /s back to \s.
 // TODO: should it?
inline UniqueString decode_path (Str s) { return iri::decode(s); }

 // Returns everything after the last slash.
Str path_filename (Str) noexcept;

 // Return the path without the filename at the end.  The return value always
 // ends in a slash (unless the input is empty).
 //    path_chop_filename("foo/bar") == "foo/"
 //    path_chop_filename("foo/") == "foo/"
 //    path_chop_filename("foo") == "./"
 //    path_chop_filename("") == ""
Str path_chop_filename (Str) noexcept;

 // Same as above, but without the trailing slash.
 //    path_chop_last_slash("foo/bar") == "foo"
 //    path_chop_last_slash("foo/") == "foo"
 //    path_chop_last_slash("foo") == "."
 //    path_chop_last_slash("/") == ""
 //    path_chop_last_slash("") == ""
Str path_chop_last_slash (Str) noexcept;

 // Get the filename extension, if any; that is, everything after the last dot
 // in the last segment of the path, or empty if there is no dot.  Does not
 // include the dot.  If a filename starts with a dot, that dot doesn't count.
Str path_extension (Str) noexcept;

///// FILE SCHEME IRIS

constexpr IRI file_scheme ("file:");

 // Get a file: IRI corresponding to the working directory the program was run
 // from.  WARNING: this is cached and only generated once, because getting the
 // current directory is surprisingly slow.  You shouldn't be chdiring in the
 // middle of a program anyway, for the same reason.
const IRI& working_directory () noexcept;

 // If you absolutely must chdir after calling working_directory(), call this to
 // update the cached IRI.
void update_working_directory () noexcept;

 // Get a file: IRI corresponding to the location of the currently running
 // program.  This is also only generated once, but if you somehow manage to
 // change the location of the program while running it, you deserve whatever
 // chaos you get.
 //
 // Use program_location().chop_filename() to get the directory containing
 // the program.
const IRI& program_location () noexcept;

///// TO/FROM FILESYSTEM PATHS

 // Create an IRI from an OS filesystem path.  Will be converted to absolute
 // form, then appended to file_scheme.  There will be no (empty) authority
 // (file:/foo/bar, not file:///foo/bar).  If base is not provided, relative
 // paths will be resolved against working_directory().  Windows paths will look
 // like file:/c:/foo/bar.
IRI from_fs_path (Str, const IRI& base = IRI()) noexcept;

 // Get a path from the given IRI.  The IRI must start with file:/ and must not
 // have an non-empty authority or a query or fragment.  If the path is a
 // directory, the trailing / will not be chopped off.
UniqueString to_fs_path (const IRI&) noexcept;

} // iri
