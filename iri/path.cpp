#include "path.h"

#include <filesystem>
namespace fs = std::filesystem;

namespace iri {

 // There's probably a better way but eh
constexpr bool on_windows = std::is_same_v<fs::path::value_type, wchar_t>;

UniqueString encode_path (Str input) noexcept {
    if (!input) return "";
    usize cap = input.size();
    for (auto c : input) {
        switch (c) {
            case IRI_FORBIDDEN: case IRI_IFFY:
            case '?': case '#': case '%': cap += 2;
            default: break;
        }
    }
    char* buf = SharableBuffer<char>::allocate(cap);
    char* out = buf;
    for (auto c : input) {
        switch (c) {
            case IRI_FORBIDDEN: case IRI_IFFY:
            case '?': case '#': case '%': {
                if constexpr (on_windows) {
                    if (c == '\\') {
                        *out++ = '/'; break;
                    }
                }
                uint8 high = uint8(c) >> 4;
                uint8 low = uint8(c) & 0xf;
                *out++ = '%';
                *out++ = high >= 10 ? high - 10 + 'A' : high + '0';
                *out++ = low >= 10 ? low - 10 + 'A' : low + '0';
                break;
            }
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
    if constexpr (on_windows) {
         // Gotta work around Windows' weird absolute path format.  This
         // code is untested and also assumes that the provided path is a
         // valid Windows path.  If not, unintuitive results may occur.
        auto encoded = encode_path(path);
        if (encoded.size() >= 2 && encoded[1] == ':') {
             // We have a drive letter.
            return IRI(cat('/', encoded));
        }
        else if (encoded[0] == '/') {
             // Who uses drive-relative paths?
             // Uh...get the drive letter from the base IRI I guess
            auto base_path = base.path();
            require(base_path.size() >= 3 &&
                base_path[0] == '/' && base_path[2] == ':'
            );
            return IRI(cat(base_path.slice(0, 3), encoded));
        }
        else {
             // Ordinary relative path
            return IRI(encoded, base);
        }
    }
    else if (!base.empty()) {
        return IRI(encode_path(path), base);
    }
    else {
         // Might be either std::u8string or const std::u8string&
        decltype(auto) abs = fs::absolute(path).generic_u8string();
        expect(!abs.empty());
        expect(abs[0] == '/' && (abs.size() == 1 || abs[1] != '/'));
        return IRI(encode_path(Str(abs)), file_scheme);
    }
}

UniqueString to_fs_path (const IRI& iri) noexcept {
    require(iri.scheme() == "file");
    require(!iri.authority());
    require(iri.hierarchical());
    require(!iri.has_query() && !iri.has_fragment());
    if constexpr (on_windows) {
         // chop initial / (its existence is guaranteed by hierarchical())
        return decode(iri.path().slice(1));
    }
    else return decode(iri.path());
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
    is(from_fs_path("/foo/bar?baz").spec(), "file:/foo/bar%3Fbaz", "from_fs_path");
    is(to_fs_path(IRI("file:/foo/bar%23baz")), "/foo/bar#baz", "to_fs_path");

    done_testing();
});

#endif
