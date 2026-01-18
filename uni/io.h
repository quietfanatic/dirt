// This module contains functions for file IO

#pragma once

#include <dirent.h>
#include <fcntl.h>

#include "callback-ref.h"
#include "common.h"
#include "errors.h"
#include "strings.h"

namespace uni {

///// FILE IO

 // RAII file object that throws exceptions.  Does not keep the path, because
 // it's only useful for error reporting.  Instead, pass the path to read() and
 // write() if you want good error messages.
struct File {
    FILE* handle;
     // Empty object
    constexpr File () : handle(null) { }
     // Open file, throws on failure
    explicit File (AnyString, const char* mode = "rb");
     // Move construct
    constexpr File (File&& o) : handle(o.handle) { o.handle = null; }
     // Move assign
    constexpr File& operator= (File&& o) {
        if (handle) close();
        handle = o.handle; o.handle = null;
        return *this;
    }
     // Close file
    constexpr ~File () { if (handle) close(); }

     // Check if open
    constexpr explicit operator bool () { return handle; }

     // Doesn't throw on failure, instead returns empty and sets errno.  If you
     // don't like the errno you get, call raise_open_failed.
    static File try_open (AnyString path, const char* mode = "rb") noexcept;

    [[noreturn]] void raise_open_failed (
        Str path_err = "", int errnum = 0
    ) const;

    UniqueString read (Str path_err = "");
    void write (Str, Str path_err = "");

     // Warns to stderr on failure.  Usually called automatically.
    void close (Str path_err = "") noexcept;
};

 // One-step file IO
UniqueString string_from_file (AnyString path);

void string_to_file (Str, AnyString path);

constexpr ErrorCode e_OpenFailed = "uni::e_OpenFailed";
constexpr ErrorCode e_ReadFailed = "uni::e_ReadFailed";
constexpr ErrorCode e_WriteFailed = "uni::e_WriteFailed";
constexpr ErrorCode e_CloseFailed = "uni::e_CloseFailed";

///// DIRECTORY IO

 // Analogous to File
struct Dir {
    DIR* handle;
    int fd;
     // Empty object
    constexpr Dir () : handle(null), fd(0) { }
     // Open from path
    explicit Dir (AnyString p);
     // Move construct
    constexpr Dir (Dir&& o) :
        handle(o.handle), fd(o.fd)
    { o.handle = null; o.fd = 0; }
     // Move assign
    constexpr Dir& operator= (Dir&& o) {
        if (handle) close();
        handle = o.handle; o.handle = null;
        fd = o.fd; o.fd = 0;
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

    [[noreturn]] void raise_open_failed (
        Str path_err = "", int errnum = 0
    ) const;

     // Get everything including . and ..
    UniqueArray<UniqueString> list (Str path_err = "");

     // Get one dirent (you probably want list or a range loop instead).
    dirent* list_one (Str path_err = "");

     // Minimum interface to allow range loops.  TODO: const char*
    struct iterator {
        Dir& self;
        dirent* entry;
        Str operator* () const { return (const char*)entry->d_name; }
        iterator& operator++ () { entry = self.list_one(); return *this; }
        bool operator != (iterator o) const { return entry != o.entry; }
    };
    iterator begin (Str path_err = "") {
        return iterator{*this, list_one(path_err)};
    }
    iterator end () {
        return iterator{*this, null};
    }

     // Warns to stderr on failure.  Usually called automatically.
    void close (Str path_err = "") noexcept;
};

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

///// INLINES

namespace in {
    [[noreturn, gnu::cold]]
    void raise_io_error (ErrorCode code, StaticString details, Str path);
    [[gnu::cold]]
    void warn_close_failed (StaticString message, Str path);
}

inline File::File (AnyString path, const char* mode) :
    File(try_open(path, mode))
{
    if (!handle) raise_open_failed(path);
}

inline File File::try_open (AnyString path, const char* mode) noexcept {
    File r;
    r.handle = fopen_utf8(path.c_str(), mode);
    return r;
}

inline void File::write (Str content, Str path_err) {
    usize did_write = fwrite(content.data(), 1, content.size(), handle);
    if (did_write != content.size()) {
        in::raise_io_error(e_WriteFailed, "Failed to write to ", path_err);
    }
}

inline void File::close (Str path_err) noexcept {
    int res = fclose(handle);
    handle = null;
    if (res != 0) [[unlikely]] {
        in::warn_close_failed("Warning: Failed to close ", path_err);
    }
}

inline UniqueString string_from_file (AnyString path) {
    return File(move(path)).read();
}

inline void string_to_file (Str content, AnyString path) {
    File(path, "wb").write(content, path);
}

inline Dir::Dir (AnyString path) :
    Dir(try_open_at(AT_FDCWD, path))
{
    if (!handle) raise_open_failed(path);
}

inline Dir Dir::try_open_at (int parent_fd, AnyString path) noexcept {
    Dir r;
    r.fd = openat(parent_fd, path.c_str(), O_RDONLY|O_DIRECTORY);
    if (r.fd >= 0) {
        r.handle = fdopendir(r.fd);
    }
    else r.handle = null;
    return r;
}

inline dirent* Dir::list_one (Str path_err) {
    errno = 0;
    dirent* r = readdir(handle);
    if (errno) {
        in::raise_io_error(e_ListDirFailed, "Failed to list directory ", path_err);
    }
    return r;
}

inline void Dir::close (Str path_err) noexcept {
    int res = closedir(handle);
    handle = null;
    fd = 0;
    if (res < 0) [[unlikely]] {
        in::warn_close_failed("Warning: Failed to close directory ", path_err);
    }
}

} // namespace ayu
