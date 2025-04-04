// This module contains functions for file IO

#pragma once

#include "callback-ref.h"
#include "common.h"
#include "errors.h"
#include "strings.h"

namespace uni {

///// FILE IO

 // RAII file object that throws exceptions.
struct File {
    FILE* ansi;
     // Only kept for error reporting purposes.
    AnyString path;
     // Empty object
    constexpr File () : ansi(null) { }
     // Open file
    explicit File (AnyString, const char* mode = "rb");
     // Take ownership of already-open file
    explicit File (FILE* f, AnyString p = "(unknown filename)") :
        ansi(f), path(move(p))
    { expect(f); }
     // Move construct
    constexpr File (File&& o) :
        ansi(o.ansi), path(move(o.path))
    { o.ansi = null; }
     // Move assign
    constexpr File& operator= (File&& o) {
        if (ansi) close_internal(ansi, path);
        path.~AnyString();
        ansi = o.ansi; o.ansi = null;
        path.impl = o.path.impl; o.path.impl = {};
        return *this;
    }
     // Close file
    constexpr ~File () { if (ansi) close_internal(ansi, path); }

    UniqueString read ();
    void write (Str);

    static void close_internal (FILE*, Str);
};

 // One-step file IO
UniqueString string_from_file (AnyString filename);

void string_to_file (Str, AnyString filename);

constexpr ErrorCode e_OpenFailed = "uni::e_OpenFailed";
constexpr ErrorCode e_ReadFailed = "uni::e_ReadFailed";
constexpr ErrorCode e_WriteFailed = "uni::e_WriteFailed";
constexpr ErrorCode e_CloseFailed = "uni::e_CloseFailed";

///// DIRECTORY IO

 // Called with fd, filename.
 //   - dir_fd: the fd of the directory, not the file's inode.  This will be the
 //             same for every call to the callback.  It can be passed to
 //             iterate_dir_at_dir.
 //   - name: just the filename with no /s.  Every filename will be passed,
 //           including . and ..
using IterateDirCallback = CallbackRef<void(int dir_fd, Str name)>;

void iterate_dir (AnyString, IterateDirCallback);

 // Returns false if not dir.  Pass AT_FDCWD for parent to open relative to the
 // current working directory.
bool iterate_if_dir_at (int parent, AnyString, IterateDirCallback);

constexpr ErrorCode e_ListDirFailed = "uni::e_ListDirFailed";

///// CONSOLE IO

 // Print UTF-8 formatted text to stdout and flushes
void print_utf8 (Str s) noexcept;
 // Prints to stderr and flushes.
void warn_utf8 (Str s) noexcept;

///// LOWER LEVEL

 // fopen but UTF-8 even on Windows.  Use fwrite to write UTF-8 text.
std::FILE* fopen_utf8 (const char* filename, const char* mode = "rb") noexcept;

 // Delete a file
int remove_utf8 (const char* filename) noexcept;

} // namespace ayu
