#include "io.h"

#include <dirent.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include "utf.h"
#endif

namespace uni {

namespace in {

[[gnu::cold]] NOINLINE static
void warn_close_failed (StaticString message, Str path) {
    warn_utf8(cat(
        message, path, ": ", strerror(errno), '\n'
    ));
}

[[noreturn, gnu::cold]] NOINLINE static
void raise_io_error (ErrorCode code, StaticString details, Str path) {
    raise(code, cat(
        details, path, ": ", strerror(errno)
    ));
}

} using namespace in;

///// FILE IO

File::File (AnyString p, const char* mode) :
    File(try_open(move(p), mode))
{
    if (!handle) raise_open_failed();
}

File File::try_open (AnyString p, const char* mode) noexcept {
    File r;
    r.path = move(p);
    r.handle = fopen_utf8(r.path.c_str(), mode);
    return r;
}

[[gnu::cold]]
void File::raise_open_failed (int errnum) const {
    errno = errnum;
    raise_io_error(e_OpenFailed, "Failed to open ", path);
}

UniqueString File::read () {
     // Find how big the file is and preallocate
    int res = fseek(handle, 0, SEEK_END);
    if (res < 0) {
         // Reading from unseekable files is NYI
        seek_failed:
        raise_io_error(e_ReadFailed, "Failed to fseek ", path);
    }
    long size = ftell(handle);
    if (size < 0) {
        raise_io_error(e_ReadFailed, "Failed to ftell ", path);
    }
    require(usize(size) < AnyString::max_size_);
    auto r = UniqueString(Uninitialized(size));
     // Reset position
    res = fseek(handle, 0, SEEK_SET);
    if (res < 0) goto seek_failed;
     // Read
    usize did_read = fread(r.data(), 1, r.size(), handle);
    if (did_read != r.size()) {
        raise_io_error(e_ReadFailed, "Failed to read from ", path);
    }
    return r;
}

void File::write (Str content) {
    usize did_write = fwrite(content.data(), 1, content.size(), handle);
    if (did_write != content.size()) {
        raise_io_error(e_WriteFailed, "Failed to write to ", path);
    }
}

void File::close () noexcept {
    int res = fclose(handle);
    handle = null;
    if (res != 0) [[unlikely]] {
        warn_close_failed("Warning: Failed to close ", path);
    }
}

UniqueString string_from_file (AnyString path) {
    return File(path).read();
}

void string_to_file (Str content, AnyString path) {
    File(path, "wb").write(content);
}

///// DIRECTORY IO

Dir::Dir (AnyString path) :
    Dir(try_open_at(AT_FDCWD, move(path)))
{
    if (!handle) raise_open_failed();
}

Dir Dir::try_open_at (int parent_fd, AnyString path) noexcept {
    Dir r;
    r.path = move(path);
    r.parent_fd = parent_fd;
    r.fd = openat(r.parent_fd, r.path.c_str(), O_RDONLY|O_DIRECTORY);
    if (r.fd >= 0) {
        r.handle = fdopendir(r.fd);
    }
    else r.handle = null;
    return r;
}

void Dir::raise_open_failed (int errnum) const {
    errno = errnum;
    raise_io_error(e_ListDirFailed, "Failed to open directory ", path);
}

UniqueArray<UniqueString> Dir::list () {
    UniqueArray<UniqueString> r;
    for (Str child : *this) {
        r.emplace_back(child);
    }
    return r;
}

dirent* Dir::list_one () {
    errno = 0;
    dirent* r = readdir(handle);
    if (errno) {
        raise_io_error(e_ListDirFailed, "Failed to list directory ", path);
    }
    return r;
}

void Dir::close () noexcept {
    int res = closedir(handle);
    handle = null;
    fd = 0;
    if (res < 0) [[unlikely]] {
        warn_close_failed("Warning: Failed to close directory ", path);
    }
}

///// CONSOLE IO

void print_utf8 (Str s) noexcept {
#ifdef _WIN32
    [[maybe_unused]] static auto set = _setmode(_fileno(stdout), _O_WTEXT);
    auto s16 = to_utf16(s);
    auto len = fwrite(s16.data(), 2, s16.size(), stdout);
    require(len == s16.size());
#else
    auto len = fwrite(s.data(), 1, s.size(), stdout);
    require(len == s.size());
#endif
    fflush(stdout);
}

void warn_utf8 (Str s) noexcept {
#ifdef _WIN32
    [[maybe_unused]] static auto set = _setmode(_fileno(stderr), _O_WTEXT);
    auto s16 = to_utf16(s);
    auto len = fwrite(s16.data(), 2, s16.size(), stderr);
    require(len == s16.size());
#else
    auto len = fwrite(s.data(), 1, s.size(), stderr);
    require(len == s.size());
#endif
    fflush(stderr);
}

///// LOWER LEVEL

std::FILE* fopen_utf8 (const char* filename, const char* mode) noexcept {
#ifdef _WIN32
    static_assert(sizeof(wchar_t) == sizeof(char16));
    return _wfopen(
        reinterpret_cast<const wchar_t*>(to_utf16(filename).c_str()),
        reinterpret_cast<const wchar_t*>(to_utf16(mode).c_str())
    );
#else
    return fopen(filename, mode);
#endif
}

int remove_utf8 (const char* filename) noexcept {
#ifdef _WIN32
    return _wremove(
        reinterpret_cast<const wchar_t*>(to_utf16(filename).c_str())
    );
#else
    return remove(filename);
#endif
}

} // uni
