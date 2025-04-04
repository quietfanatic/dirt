// This module contains functions for file IO

#pragma once

#include <dirent.h>

#include "callback-ref.h"
#include "common.h"
#include "errors.h"
#include "strings.h"

namespace uni {

///// FILE IO

 // RAII file object that throws exceptions.
struct File {
    FILE* handle;
     // Only kept for error reporting purposes.
    AnyString path;
     // Empty object
    constexpr File () : handle(null) { }
     // Open file, throws on failure
    explicit File (AnyString, const char* mode = "rb");
     // Move construct
    constexpr File (File&& o) :
        handle(o.handle), path(move(o.path))
    { o.handle = null; }
     // Move assign
    constexpr File& operator= (File&& o) {
        if (handle) close();
        path.~AnyString();
        handle = o.handle; o.handle = null;
        path.impl = o.path.impl; o.path.impl = {};
        return *this;
    }
     // Close file
    constexpr ~File () { if (handle) close(); }

     // Check if open
    constexpr explicit operator bool () { return handle; }

     // Doesn't throw on failure, instead returns empty and sets errno.  If you
     // don't like the errno you get, call raise_open_failed.
    static File try_open (AnyString path, const char* mode = "rb") noexcept;

    [[noreturn]] void raise_open_failed (int errnum = errno) const;

    UniqueString read ();
    void write (Str);

     // Warns to stderr on failure.  Usually called automatically.
    void close () noexcept;
};

 // One-step file IO
UniqueString string_from_file (AnyString filename);

void string_to_file (Str, AnyString filename);

constexpr ErrorCode e_OpenFailed = "uni::e_OpenFailed";
constexpr ErrorCode e_ReadFailed = "uni::e_ReadFailed";
constexpr ErrorCode e_WriteFailed = "uni::e_WriteFailed";
constexpr ErrorCode e_CloseFailed = "uni::e_CloseFailed";

///// DIRECTORY IO

 // Analogous to File
struct Dir {
    DIR* handle;
    int fd;
     // Only kept for error reporting purposes.
    int parent_fd;
    AnyString path;
     // Empty object
    constexpr Dir () : handle(null), fd(0) { }
     // Open from path
    explicit Dir (AnyString p);
     // Move construct
    constexpr Dir (Dir&& o) :
        handle(o.handle), fd(o.fd), parent_fd(o.parent_fd), path(move(o.path))
    { o.handle = null; o.fd = 0; o.parent_fd = 0; }
     // Move assign
    constexpr Dir& operator= (Dir&& o) {
        if (handle) close();
        path.~AnyString();
        handle = o.handle; o.handle = null;
        fd = o.fd; o.fd = 0;
        parent_fd = o.parent_fd; o.parent_fd = 0;
        path.impl = o.path.impl; o.path.impl = {};
        return *this;
    }
     // Close file
    constexpr ~Dir () { if (handle) close(); }

     // Check openness
    constexpr explicit operator bool () const { return handle; }

     // Opens relative to an already open directory, or the cwd if you pass
     // AT_FDCWD from <fcntl.h>.  Doesn't throw, but returns empty and sets
     // errno.  If you don't like the errno you get, call raise_open_failed().
     // TODO: avoid extra string copy when recursing
    static Dir try_open_at (int parent_fd, AnyString path) noexcept;

    [[noreturn]] void raise_open_failed (int errnum = errno) const;

     // Get everything including . and ..
    UniqueArray<UniqueString> list ();

     // Get one dirent (you probably want read() or a range loop instead).
    dirent* list_one ();

     // Minimum interface to allow range loops.  TODO: const char*
    struct iterator {
        Dir& self;
        dirent* entry;
        Str operator* () const { return (const char*)entry->d_name; }
        iterator& operator++ () { entry = self.list_one(); return *this; }
        bool operator != (iterator o) const { return entry != o.entry; }
    };
    iterator begin () {
        return iterator{*this, list_one()};
    }
    iterator end () {
        return iterator{*this, null};
    }

     // Warns to stderr on failure.  Usually called automatically.
    void close () noexcept;
};

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
