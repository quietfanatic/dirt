#include "path.h"

#include <filesystem>
namespace fs = std::filesystem;

namespace iri {

UniqueString encode_path (Str input) noexcept {
    if (!input) return "";
    usize cap = input.size();
    for (auto c : input) {
        switch (c) {
            case '?': case '#': case '%': cap += 2;
            default: break;
        }
    }
    char* buf = SharableBuffer<char>::allocate(cap);
    char* out = buf;
    for (auto c : input) {
        switch (c) {
            case '?':
                *out++ = '%'; *out++ = '3'; *out++ = 'F'; break;
            case '#':
                *out++ = '%'; *out++ = '2'; *out++ = '3'; break;
            case '%':
                *out++ = '%'; *out++ = '2'; *out++ = '5'; break;
            default: *out++ = c; break;
        }
    }
    return UniqueString::UnsafeConstructOwned(buf, out - buf);
}

Str path_without_filename (Str path) noexcept {
    if (!path) return path;
    for (const char* p = path.end(); p != path.begin(); --p) {
        if (p[-1] == '/') return Str(path.begin(), p);
    }
    return "./";
}

Str path_parent (Str path) noexcept {
    if (!path) return path;
    Str wf = path_without_filename(path);
    return wf.shrunk(wf.size() - 1);
}

Str path_extension (Str path) noexcept {
    for (const char* p = path.end(); p != path.begin(); --p) {
        if (p[-1] == '.') return Str(p, path.end());
        else if (p[-1] == '/') return "";
    }
    return "";
}

IRI from_fs_path (Str path, const IRI& base) noexcept {
    if (!path) return IRI();
    if (!base.empty()) {
         // TODO: this won't work correctly with Windows drive letters.
        return IRI(encode_path(path), base);
    }
    else {
         // Might be either std::u8string or const std::u8string&
        decltype(auto) abs = fs::absolute(path).generic_u8string();
        expect(!abs.empty());
         // File root already has a / for its path, so it'll work (kinda
         // accidentally) with Windows drive letters.
        return IRI(encode_path(Str(abs)), file_root);
    }
}

IRI in::from_fs_path_sfp (const std::filesystem::path& path, const IRI& base) noexcept {
    if (path.empty()) return IRI();
    if (!base.empty()) {
         // TODO: this won't work correctly with Windows drive letters.
        return IRI(encode_path(Str(path.generic_u8string())), base);
    }
    else {
         // Might be either std::u8string or const std::u8string&
        decltype(auto) abs = fs::absolute(path).generic_u8string();
        expect(!abs.empty());
         // File root already has a / for its path, so it'll work (kinda
         // accidentally) with Windows drive letters.
        return IRI(encode_path(Str(abs)), file_root);
    }
}

UniqueString to_fs_path (const IRI& file_iri) noexcept {
    require(file_iri.scheme() == "file");
    require(file_iri.has_authority() && !file_iri.authority());
    require(!file_iri.has_query() && !file_iri.has_fragment());
    return decode(file_iri.path());
}

} // iri

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

static tap::TestSet tests ("dirt/iri/path", []{
    using namespace tap;
    using namespace iri;

    is(encode_path("foo/bar?qux#tal"), "foo/bar%3Fqux%23tal", "encode_path");
    is(path_without_filename("foo/bar"), "foo/", "path_without_filename foo/bar");
    is(path_without_filename("foo/"), "foo/", "path_without_filename foo/");
    is(path_without_filename("foo"), "./", "path_without_filename foo");
    is(path_extension("foo/bar.baz"), "baz", "path_extension");
    is(path_extension("foo.bar/baz"), "", "path_extension none");
    is(path_extension("foo.bar/baz."), "", "path_extension trailing dot ignored");
     // TODO: Make these tests work on Windows
     // TODO: test relative paths somehow
    is(from_fs_path("/foo/bar?baz").spec(), "file:///foo/bar%3Fbaz", "from_fs_path");
    is(to_fs_path(IRI("file:///foo/bar%23baz")), "/foo/bar#baz", "to_fs_path");

    done_testing();
});

#endif
