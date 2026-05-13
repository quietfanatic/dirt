// This module contains functions for file IO

#pragma once

#include <dirent.h>
#include <fcntl.h>

#include "arrays.h"
#include "callback-ref.h"
#include "common.h"
#include "errors.h"

namespace uni {

///// FILE IO

 // RAII file object that throws exceptions.  Does not keep the path, because
 // it's only useful for error reporting.  Instead, catch the error and tag it
 // with the path.
struct File {
     // TODO: investiage if we want to use fd instead.  We already rely on POSIX
     // IO headers.
    FILE* handle;
     // Empty object
    constexpr File () : handle(null) { }
     // Open file, throws on failure
    explicit File (const char* path, const char* mode = "rb");
    explicit File (Str path, const char* mode = "rb");
     // Move construct
    constexpr File (File&& o) : handle(o.handle) { o.handle = null; }
     // Move assign
    constexpr File& operator= (File&& o) {
        if (handle) close();
        handle = o.handle; o.handle = null;
        return *this;
    }
     // Autoclose file (close manually if you need error information).
    constexpr ~File () { if (handle) close_warn(); }

     // Check if open
    constexpr explicit operator bool () { return handle; }

     // Doesn't throw on failure, instead returns empty and sets errno.  If you
     // don't like the errno you get, call raise_open_failed
    static File try_open (const char* path, const char* mode = "rb") noexcept;
    static File try_open (Str path, const char* mode = "rb") noexcept;
     // (this).  If passed non-zero, will set errno, otherwise will use existing
     // errno.
    [[noreturn]] void raise_open_failed (int errnum = 0) const;

     // Get the entire file's contents in a string.  Will throw on failure.
    UniqueString read ();
     // Write the entire file's contents.  Will throw on failure.
    void write (Str);

     // If closing fails, will throw an exception.
    void close () { close_throw(); expect(!handle); }
    void close_throw ();
     // If closing fails, will warn instead of throwing.  Note that there is no
     // way to recover the filename.  If you need better error diagnostics, then
     // call .close() before letting the File destroy itself and catch it then.
     // TODO: pluggable on_close_failed function.
    void close_warn () noexcept;
};

 // One-step file IO.  Automatically tags errno with uni::FilePath.
UniqueString string_from_file (const char* path);
UniqueString string_from_file (Str path);

void string_to_file (Str content, const char* path);
void string_to_file (Str content, Str path);

 // The code for all IO errors.
constexpr ErrorCode e_IOError = "uni::e_IOError";

///// DIRECTORY IO

 // Analogous to File.
struct Dir {
    DIR* handle;
    int fd;
     // Empty object
    constexpr Dir () : handle(null), fd(0) { }
     // Open from path
    explicit Dir (const char* path);
    explicit Dir (Str path);
     // Move construct
    constexpr Dir (Dir&& o) :
        handle(o.handle), fd(o.fd)
    { o.handle = null; o.fd = 0; }
     // Move assign
    constexpr Dir& operator= (Dir&& o) {
        if (handle) close_warn();
        handle = o.handle; o.handle = null;
        fd = o.fd; o.fd = 0;
        return *this;
    }
     // Close file
    constexpr ~Dir () { if (handle) close_warn(); }

     // Check openness
    constexpr explicit operator bool () const { return handle; }

     // Opens relative to an already open directory, or the cwd if you pass
     // AT_FDCWD from <fcntl.h>.  Doesn't throw, but returns empty and sets
     // errno.  If you don't like the errno you get, call raise_open_failed().
     // TODO: avoid extra string copy when recursing
    static Dir try_open_at (int parent_fd, const char* path) noexcept;
    static Dir try_open_at (int parent_fd, Str path) noexcept;

    [[noreturn]] void raise_open_failed (int errnum = 0) const;

     // Get everything including . and ..
    UniqueArray<SharedString> list ();

     // Get one dirent (you probably want list or a range loop instead).
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

    void close () { close_throw(); expect(!handle); }
    void close_throw ();
     // Warns to stderr on failure.
    void close_warn () noexcept;
};

 // One-stop directory IO.  Gets everything including . and ..
UniqueArray<SharedString> list_dir (const char* path);
UniqueArray<SharedString> list_dir (Str path);

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

inline File File::try_open (const char* path, const char* mode) noexcept {
    File r;
    r.handle = fopen_utf8(path, mode);
    return r;
}

inline UniqueArray<u8> blob_from_file (const char* path) {
    auto s = string_from_file(path);
    UniqueArray<u8> r;
    r.impl.size = s.impl.size;
    r.impl.data = (u8*)s.impl.data;
    s.impl = {};
    return r;
}
inline UniqueArray<u8> blob_from_file (Str path) {
    auto s = string_from_file(path);
    UniqueArray<u8> r;
    r.impl.size = s.impl.size;
    r.impl.data = (u8*)s.impl.data;
    s.impl = {};
    return r;
}

inline void blob_to_file (Slice<u8> content, const char* path) {
    string_to_file(content.reinterpret<char>(), path);
}
inline void blob_to_file (Slice<u8> content, Str path) {
    string_to_file(content.reinterpret<char>(), path);
}

} // namespace ayu
