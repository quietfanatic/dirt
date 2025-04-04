#include "io.h"

#include <dirent.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include "utf.h"
#endif

namespace uni {

namespace in {

[[noreturn, gnu::cold]] NOINLINE static
void raise_io_error (ErrorCode code, StaticString details, Str filename, int errnum) {
    raise(code, cat(
        details, filename, ": ", strerror(errnum)
    ));
}

} using namespace in;

///// FILE IO

File::File (AnyString path, const char* mode) :
    ansi(fopen_utf8(path.c_str(), mode)),
    path(move(path))
{
    expect(mode && mode[0]);
    if (!ansi) {
        raise_io_error(e_OpenFailed, "Failed to open ", path, errno);
    }
}

void File::close_internal (FILE* ansi, Str path) {
    int res = fclose(ansi);
    if (res != 0) [[unlikely]] {
        warn_utf8(cat(
            "Warning: Failed to close ", path, ": ", strerror(errno), '\n'
        ));
    }
}

UniqueString File::read () {
     // Find how big the file is and preallocate
    int res = fseek(ansi, 0, SEEK_END);
    if (res < 0) {
         // Reading from unseekable files is NYI
        seek_failed:
        raise_io_error(e_ReadFailed, "Failed to fseek ", path, errno);
    }
    long size = ftell(ansi);
    if (size < 0) {
        raise_io_error(e_ReadFailed, "Failed to ftell ", path, errno);
    }
    require(usize(size) < AnyString::max_size_);
    auto r = UniqueString(Uninitialized(size));
     // Reset position
    res = fseek(ansi, 0, SEEK_SET);
    if (res < 0) goto seek_failed;
     // Read
    usize did_read = fread(r.data(), 1, r.size(), ansi);
    if (did_read != r.size()) {
        raise_io_error(e_ReadFailed, "Failed to read from ", path, errno);
    }
    return r;
}

void File::write (Str content) {
    usize did_write = fwrite(content.data(), 1, content.size(), ansi);
    if (did_write != content.size()) {
        raise_io_error(e_WriteFailed, "Failed to write to ", path, errno);
    }
}

UniqueString string_from_file (AnyString path) {
    return File(path).read();
}

void string_to_file (Str content, AnyString path) {
    File(path, "wb").write(content);
}

///// DIRECTORY IO

void iterate_dir (AnyString dirname, IterateDirCallback cb) {
    bool ret = iterate_if_dir_at(AT_FDCWD, move(dirname), cb);
    if (!ret) {
        raise_io_error(e_ListDirFailed, "Failed to open directory ", dirname, ENOTDIR);
    }
}

bool iterate_if_dir_at (int parent, AnyString dirname, IterateDirCallback cb) {
    int fd = openat(parent, dirname.c_str(), O_RDONLY|O_DIRECTORY);
    if (fd < 0) {
        int errnum = errno;
        if (errnum == ENOTDIR) return false;
        raise_io_error(e_ListDirFailed, "Failed to open directory ", dirname, errnum);
    }
    DIR* dir = fdopendir(fd);
    if (!dir) {
        int errnum = errno;
        raise_io_error(e_ListDirFailed, "Failed to fdopendir directory ", dirname, errnum);
    }
    for (;;) {
        errno = 0;
        dirent* entry = readdir(dir);
        if (errno) {
            int errnum = errno;
            closedir(dir);
            raise_io_error(e_ListDirFailed, "Failed to list directory ", dirname, errnum);
        }
        if (!entry) break;
        try {
            cb(fd, (const char*)entry->d_name);
        } catch (...) { closedir(dir); throw; }
    }
    if (closedir(dir)) {
        int errnum = errno;
        raise_io_error(e_ListDirFailed, "Failed to close directory ", dirname, errnum);
    }
    return true;
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
