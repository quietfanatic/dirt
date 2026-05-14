#include "io.h"

#include <dirent.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include "utf.h"
#endif

namespace uni {

namespace in {

[[noreturn, gnu::cold]] NOINLINE
void raise_io_error (const char* op) try {
    throw Error(e_IOError, "IO operation failed");
}
catch (Error& e) {
    e.add_tag("uni::IOOperation", StaticString(op));
    e.rethrow_with_tag("strerror(errno)", strerror(errno));
}

[[gnu::cold]] NOINLINE
void warn_close_failed (StaticString thing) {
    warn_utf8(cat(
        "WARNING: ignoring failure to close a ", thing, ": ",  strerror(errno), '\n'
    ));
}

} using namespace in;

NOINLINE
File::File (const char* path, const char* mode) :
    File(try_open(path, mode))
{
    if (!handle) try {
        raise_io_error("open");
    } catch (Error& e) { e.rethrow_with_tag("uni::FilePath", path); }
}
[[gnu::no_stack_protector]] NOINLINE
File::File (Str path, const char* mode) {
    with_c_str(path, [&](auto cs){
        new (this) File(cs, mode);
    });
}

[[gnu::no_stack_protector]] NOINLINE
File File::try_open (Str path, const char* mode) noexcept {
    return with_c_str(path, [&](auto cs){
        return try_open(cs, mode);
    });
}

[[noreturn, gnu::cold]] NOINLINE
void File::raise_open_failed (int errnum) const {
    if (errnum) errno = errnum;
    raise_io_error("open");
}

UniqueString File::read () {
     // Find how big the file is and preallocate
    int res = fseek(handle, 0, SEEK_END);
    if (res < 0) {
         // Reading from unseekable files is NYI
        seek_failed:
        raise_io_error("fseek");
    }
    long size = ftell(handle);
    if (size < 0) {
        raise_io_error("ftell");
    }
    require(usize(size) < SharedString::max_size_);
    auto r = UniqueString(Uninitialized(size));
     // Reset position
    res = fseek(handle, 0, SEEK_SET);
    if (res < 0) goto seek_failed;
     // Read
    usize did_read = fread(r.data(), 1, r.size(), handle);
    if (did_read != r.size()) {
        raise_io_error("read");
    }
    return r;
}

void File::write (Str content) {
    usize did_write = fwrite(content.data(), 1, content.size(), handle);
    if (did_write != content.size()) {
        in::raise_io_error("write");
    }
}

void File::close_throw () {
    int res = fclose(handle);
    handle = null;
    if (res != 0) raise_io_error("close");
}

void File::close_warn () noexcept {
    int res = fclose(handle);
    handle = null;
    if (res != 0) [[unlikely]] warn_close_failed("file");
}

NOINLINE
UniqueString string_from_file (const char* path) try {
    File f = File::try_open(path);
    if (!f) raise_io_error("open");
    UniqueString r = f.read();
    f.close();
    return r;
} catch (Error& e) {
    e.rethrow_with_tag("uni::FilePath", path);
}
[[gnu::no_stack_protector]] NOINLINE
UniqueString string_from_file (Str path) {
    return with_c_str(path, [](auto cs){
        return string_from_file(cs);
    });
}

NOINLINE
void string_to_file (const char* path, Str content) try {
    File f = File::try_open(path, "wb");
    if (!f) raise_io_error("open");
    f.write(content);
    f.close();
}
catch (Error& e) {
    e.rethrow_with_tag("uni::FilePath", path);
}
[[gnu::no_stack_protector]] NOINLINE
void string_to_file (Str path, Str content) {
    with_c_str(path, [&](auto cs){
        string_to_file(cs, content);
    });
}

NOINLINE
Dir::Dir (const char* path) :
    Dir(try_open_at(AT_FDCWD, path))
{
    if (!handle) try {
        raise_io_error("open dir");
    } catch (Error& e) { e.rethrow_with_tag("uni::FilePath", path); }
}
[[gnu::no_stack_protector]] NOINLINE
Dir::Dir (Str path) {
    with_c_str(path, [&](auto cs){
        new (this) Dir(cs);
    });
}

NOINLINE
Dir Dir::try_open_at (int parent_fd, const char* path) noexcept {
    Dir r;
    int fd = openat(parent_fd, path, O_RDONLY|O_DIRECTORY);
    if (fd < 0) return Dir();
    return Dir(fdopendir(fd), int(fd));
}
[[gnu::no_stack_protector]] NOINLINE
Dir Dir::try_open_at (int parent_fd, Str path) noexcept {
    return with_c_str(path, [&](auto cs){
        return Dir::try_open_at(parent_fd, cs);
    });
}

UniqueArray<SharedString> Dir::list () {
    UniqueArray<SharedString> r;
    while (1) {
        dirent* entry = list_one();
        if (!entry) return r;
        r.emplace_back(entry->d_name);
    }
}

dirent* Dir::list_one () {
    errno = 0;
    dirent* r = readdir(handle);
    if (!r && errno) {
        in::raise_io_error("list dir");
    }
    return r;
}

void Dir::close_throw () {
    int res = closedir(handle);
    handle = null;
    fd = 0;
    if (res < 0) raise_io_error("close dir");
}

void Dir::close_warn () noexcept {
    int res = closedir(handle);
    handle = null;
    fd = 0;
    if (res < 0) [[unlikely]] warn_close_failed("directory");
}

 // One-stop directory IO
NOINLINE
UniqueArray<SharedString> list_dir (const char* path) try {
    Dir d = Dir::try_open_at(AT_FDCWD, path);
    if (!d) raise_io_error("open dir");
    UniqueArray<SharedString> r = d.list();
    d.close();
    return r;
} catch (Error& e) {
    e.add_tag("uni::FilePath", path);
    throw;
}
[[gnu::no_stack_protector]] NOINLINE
UniqueArray<SharedString> list_dir (Str path) {
    return with_c_str(path, [](auto cs){
        return list_dir(cs);
    });
}

///// CONSOLE IO

void print_utf8 (Str s) noexcept {
#ifdef _WIN32
    [[maybe_unused]] static auto set = _setmode(_fileno(stdout), _O_WTEXT);
    auto s16 = to_utf16(s);
    auto len = std::fwrite(s16.data(), 2, s16.size(), stdout);
    require(len == s16.size());
#else
    auto len = std::fwrite(s.data(), 1, s.size(), stdout);
    require(len == s.size());
#endif
    std::fflush(stdout);
}

void warn_utf8 (Str s) noexcept {
#ifdef _WIN32
    [[maybe_unused]] static auto set = _setmode(_fileno(stderr), _O_WTEXT);
    auto s16 = to_utf16(s);
    auto len = sdt::fwrite(s16.data(), 2, s16.size(), stderr);
    require(len == s16.size());
#else
    auto len = std::fwrite(s.data(), 1, s.size(), stderr);
    require(len == s.size());
#endif
    std::fflush(stderr);
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
    return std::fopen(filename, mode);
#endif
}

int remove_utf8 (const char* filename) noexcept {
#ifdef _WIN32
    return _wremove(
        reinterpret_cast<const wchar_t*>(to_utf16(filename).c_str())
    );
#else
    return std::remove(filename);
#endif
}

} // uni
