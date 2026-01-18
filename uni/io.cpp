#include "io.h"

#include <dirent.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include "utf.h"
#endif

namespace uni {

namespace in {

[[gnu::cold]] NOINLINE
void warn_close_failed (StaticString message, Str path) {
    if (!path) path = "a file";
    warn_utf8(cat(
        message, path, ": ", strerror(errno), '\n'
    ));
}

[[noreturn, gnu::cold]] NOINLINE
void raise_io_error (ErrorCode code, StaticString details, Str path) {
    if (!path) path = "a file";
    raise(code, cat(
        details, path, ": ", strerror(errno)
    ));
}

} using namespace in;

[[noreturn, gnu::cold]] NOINLINE
void File::raise_open_failed (Str path, int errnum) const {
    if (errnum) errno = errnum;
    raise_io_error(e_OpenFailed, "Failed to open ", path);
}

UniqueString File::read (Str path_err) {
     // Find how big the file is and preallocate
    int res = fseek(handle, 0, SEEK_END);
    if (res < 0) {
         // Reading from unseekable files is NYI
        seek_failed:
        raise_io_error(e_ReadFailed, "Failed to fseek ", path_err);
    }
    long size = ftell(handle);
    if (size < 0) {
        raise_io_error(e_ReadFailed, "Failed to ftell ", path_err);
    }
    require(usize(size) < AnyString::max_size_);
    auto r = UniqueString(Uninitialized(size));
     // Reset position
    res = fseek(handle, 0, SEEK_SET);
    if (res < 0) goto seek_failed;
     // Read
    usize did_read = fread(r.data(), 1, r.size(), handle);
    if (did_read != r.size()) {
        raise_io_error(e_ReadFailed, "Failed to read from ", path_err);
    }
    return r;
}

void Dir::raise_open_failed (Str path_err, int errnum) const {
    if (errnum) errno = errnum;
    raise_io_error(e_ListDirFailed, "Failed to open directory ", path_err);
}

UniqueArray<UniqueString> Dir::list (Str path_err) {
    UniqueArray<UniqueString> r;
    while (1) {
        dirent* entry = list_one(path_err);
        if (!entry) return r;
        r.emplace_back(entry->d_name);
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
