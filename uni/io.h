// This module contains functions for file IO

#pragma once

#include "common.h"
#include "errors.h"
#include "strings.h"

namespace uni {

UniqueString string_from_file (AnyString filename);
UniqueString string_from_file (FILE* file, Str filename = "(unknown)");

void string_to_file (Str, AnyString filename);
void string_to_file (Str, FILE* file, Str filename = "(unknown)");

constexpr ErrorCode e_OpenFailed = "uni::e_OpenFailed";
constexpr ErrorCode e_ReadFailed = "uni::e_ReadFailed";
constexpr ErrorCode e_WriteFailed = "uni::e_WriteFailed";
constexpr ErrorCode e_CloseFailed = "uni::e_CloseFailed";

///// UTF-8 IO FUNCTIONS

 // fopen but UTF-8 even on Windows.  Use fwrite to write UTF-8 text.
std::FILE* fopen_utf8 (const char* filename, const char* mode = "rb") noexcept;

 // Print UTF-8 formatted text to stdout and flushes
void print_utf8 (Str s) noexcept;
 // Prints to stderr and flushes.
void warn_utf8 (Str s) noexcept;

 // Delete a file
int remove_utf8 (const char* filename) noexcept;

} // namespace ayu
