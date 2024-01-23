#include "io.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace uni {

namespace in {

[[noreturn, gnu::cold]] NOINLINE static
void raise_io_error (ErrorCode code, StaticString details, Str filename, int errnum) {
    raise(code, cat(
        details, filename, ": ", std::strerror(errnum)
    ));
}

} using namespace in;

FILE* open_file (AnyString filename, const char* mode) {
    FILE* file = fopen_utf8(filename.c_str(), mode);
    if (!file) {
        raise_io_error(e_OpenFailed,
            "Failed to open for reading ", filename, errno
        );
    }
    return file;
}

void close_file (FILE* file, Str filename) {
    if (fclose(file) != 0) {
        int errnum = errno;
        raise_io_error(e_CloseFailed, "Failed to close ", filename, errnum);
    }
}

UniqueString string_from_file (AnyString filename) {
    FILE* file = open_file(filename, "rb");

    UniqueString r;
    try { r = string_from_file(file, filename); }
    catch (...) { fclose(file); throw; }
    close_file(file, filename);
    return r;
}

UniqueString string_from_file (FILE* file, Str filename) {
     // Find how big the file is and preallocate
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    require(usize(size) < AnyString::max_size_);
    auto r = UniqueString(Uninitialized(size));
    rewind(file);
     // Read
    usize did_read = fread(r.data(), 1, r.size(), file);
    if (did_read != r.size()) {
        int errnum = errno;
        raise_io_error(e_ReadFailed, "Failed to read from ", filename, errnum);
    }
    return r;
}

void string_to_file (Str content, AnyString filename) {
    FILE* file = open_file(filename, "wb");

    try { string_to_file(content, file, filename); }
    catch (...) { fclose(file); throw; }
    close_file(file, filename);
}

void string_to_file (Str content, FILE* file, Str filename) {
    usize did_write = fwrite(content.data(), 1, content.size(), file);
    if (did_write != content.size()) {
        int errnum = errno;
        raise_io_error(e_WriteFailed, "Failed to write to ", filename, errnum);
    }
}

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
