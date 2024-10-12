 // This module contains cross-system compatibility functions, mostly UTF-8
 // related functions for use on Windows.  There are apparently ways to get
 // Windows programs to use UTF-8 encoding for IO, but I can't get it to work.

#pragma once

#include <cstdio>

#include "common.h"
#include "arrays.h"

namespace uni {

///// UTF-8/UTF-16 CONVERSION

 // UTF-8/UTF-16 conversion functions.  These are best-effort, and never throw
 // errors, instead passing invalid characters through.  Unmatched UTF-8 bytes
 // and overlong sequences are treated as Latin-1 characters, and unmatched
 // UTF-16 surrogates are encoded as-is into UTF-8.   UTF-16 is native-endian.

 // Convert a UTF-8 string into a native-endian UTF-16 string.
UniqueString16 to_utf16 (Str) noexcept;

 // Convert a native-endian UTF-16 string into a UTF-8 string.
UniqueString from_utf16 (Str16) noexcept;

constexpr bool is_continuation_byte (char c) { return (c & 0xc0) == 0x80; }

} // namespace uni
