#include "path.h"

#include "../uni/strings.h"
#include "../whereami/whereami.h"
#include <filesystem>
namespace fs = std::filesystem;

namespace iri {
using namespace in;

constexpr bool backwards_slashes = fs::path::preferred_separator == '\\';

///// PATH MANIPULATION

UniqueString encode_path (Str input) noexcept {
    if (!input) return "";
    usize cap = input.size();
    for (auto c : input) switch (char_behavior(c)) {
        case CharProps::Forbidden:
        case CharProps::Iffy:
        case CharProps::Question:
        case CharProps::Hash:
        case CharProps::Percent:
            if (backwards_slashes && c == '\\') break;
            cap += 2; break;
        case CharProps::Ordinary:
        case CharProps::Slash:
            break;
        default: never();
    }
     // Don't bother failing here
    //require(cap < iri::maximum_length);
    char* buf = SharableBuffer<char>::allocate(cap);
    char* out = buf;
    for (auto c : input) switch (char_behavior(c)) {
        case CharProps::Forbidden:
        case CharProps::Iffy:
        case CharProps::Question:
        case CharProps::Hash:
        case CharProps::Percent: {
            if constexpr (backwards_slashes) {
                if (c == '\\') {
                    *out++ = '/'; break;
                }
            }
            u8 high = u8(c) >> 4;
            u8 low = u8(c) & 0xf;
            *out++ = '%';
            *out++ = high >= 10 ? high - 10 + 'A' : high + '0';
            *out++ = low >= 10 ? low - 10 + 'A' : low + '0';
            break;
        }
        case CharProps::Ordinary:
        case CharProps::Slash:
            *out++ = c; break;
        default: never();
    }
    UniqueString r;
    r.impl = {u32(out - buf), buf};
    return r;
}

Str path_filename (Str path) noexcept {
    for (const char* p = path.end(); p != path.begin(); --p) {
        if (p[-1] == '/') return Str(p, path.end());
    }
    return path;
}

Str path_chop_filename (Str path) noexcept {
    if (!path) [[unlikely]] return "";
    for (const char* p = path.end(); p != path.begin(); --p) {
        if (p[-1] == '/') return Str(path.begin(), p);
    }
    return "./";
}

Str path_chop_last_slash (Str path) noexcept {
    if (!path) [[unlikely]] return "";
    Str wf = path_chop_filename(path);
    return wf.chop(wf.size() - 1);
}

Str path_extension (Str path) noexcept {
    for (const char* p = path.end(); p != path.begin(); --p) {
        if (p[-1] == '.') {
            if (p-1 == path.begin() || p[-2] == '/') {
                 // Filename starts with . and has no extension
                return "";
            }
            else return Str(p, path.end());
        }
        else if (p[-1] == '/') return "";
    }
    return "";
}

///// FILE SCHEME IRIS

static IRI current_working_directory;

const IRI& working_directory () noexcept {
     // Not threadsafe, but IRIs use unthreadsafe reference counting anyway.
    if (!current_working_directory) update_working_directory();
    return current_working_directory;
}

void update_working_directory () noexcept {
     // Make sure to tack a / on the end or relative resolving won't work
    current_working_directory = expect(from_fs_path(
        cat(fs::current_path().generic_u8string(), '/')
    ));
}

const IRI& program_location () noexcept {
    static IRI r = []{
        int len = wai_getExecutablePath(nullptr, 0, nullptr);
        require(len > 0);
        auto path = new char [len];
        require(wai_getExecutablePath(path, len, nullptr) == len);
        IRI r = from_fs_path(Str(path, len));
        expect(r);
         // Promote the IRI's AnyString to static, unless someone replaced IRI's
         // string type with something incompatible.
        if (requires { r.spec_.impl.sizex2_with_owned; }) {
            auto& sx2wo = r.spec_.impl.sizex2_with_owned;
            const_cast<usize&>(sx2wo) &= ~1;
        }
        delete[] path;
        return r;
    }();
    return r;
}

///// TO/FROM FILESYSTEM PATHS

IRI from_fs_path (Str path, const IRI& base) noexcept {
    if (!path) return IRI();
    auto encoded = encode_path(path);
    if constexpr (backwards_slashes) {
         // Gotta work around Windows' weird absolute path format.  This
         // code is untested and also assumes that the provided path is a
         // valid Windows path.  If not, unintuitive results may occur.
        if (encoded.size() >= 2 && encoded[1] == ':') {
            require(
                encoded.size() >= 3 &&
                ((encoded[0] >= 'a' && encoded[0] <= 'z') ||
                 (encoded[0] >= 'A' && encoded[0] <= 'Z')) &&
                (encoded[2] == '\\' || encoded[2] == '/')
            );
             // We have a drive letter.
            return IRI(cat('/', encoded), file_scheme);
        }
        else if (encoded[0] == '/') {
             // Who uses drive-relative paths?
             // Uh...get the drive letter from the base IRI I guess
            auto base_path = (
                !base.empty() ? base : working_directory()
            ).path();
            require(base_path.size() >= 3 &&
                base_path[0] == '/' && base_path[2] == ':'
            );
            return IRI(cat(base_path.slice(0, 3), encoded), file_scheme);
        }
        else {
             // Ordinary relative path
            return IRI(encoded, !base.empty() ? base : working_directory());
        }
    }
    else if (encoded[0] == '/') {
        Str enc = encoded;
         // Trim multiple /s so they don't get interpreted as an authority
        while (enc.size() >= 2 && enc[1] == '/') {
            enc = enc.slice(1);
        }
         // Don't call working_directory() here because it calls us.
        return IRI(enc, file_scheme);
    }
    else return IRI(
        encoded, !base.empty() ? base : working_directory()
    );
}

UniqueString to_fs_path (const IRI& iri) noexcept {
    require(iri.scheme() == "file");
    require(!iri.authority());  // authority can exist if empty
    require(iri.hierarchical());
    require(!iri.has_query() && !iri.has_fragment());
    if constexpr (backwards_slashes) {
         // chop initial / (its existence is guaranteed by hierarchical())
        return decode(iri.path().slice(1));
    }
    else return decode(iri.path());
}

} // iri

///// TESTS

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

static tap::TestSet tests ("dirt/iri/path", []{
    using namespace tap;
    using namespace iri;

    is(encode_path("foo/bar?qux#tal"), "foo/bar%3Fqux%23tal", "encode_path");
    is(path_chop_filename("foo/bar"), "foo/", "path_chop_filename foo/bar");
    is(path_chop_filename("foo/"), "foo/", "path_chop_filename foo/");
    is(path_chop_filename("foo"), "./", "path_chop_filename foo");
    is(path_extension("foo/bar.baz"), "baz", "path_extension");
    is(path_extension("foo.bar/baz"), "", "path_extension none");
    is(path_extension("foo.bar/baz."), "", "path_extension trailing dot ignored");
     // TODO: test relative paths somehow
    AnyString exp;
    if constexpr (backwards_slashes) {
        Str wd = working_directory().path();
        ok(wd.size() > 2);
        ok(wd[1] >= 'A' && wd[1] <= 'Z');
        is(wd[2], ':');
        exp = cat("file:/", wd[1], ":/foo/bar%3Fbaz");
    }
    else {
        exp = "file:/foo/bar%3Fbaz";
    }
    is(from_fs_path("/foo/bar?baz").spec(), exp, "from_fs_path");
    AnyString exp2;
    if constexpr (backwards_slashes) {
        Str wd = working_directory().path();
        exp2 = cat(wd[1], ":/foo/bar?baz");
    }
    else {
        exp2 = "/foo/bar?baz";
    }
    is(to_fs_path(IRI(exp)), exp2, "to_fs_path");

    done_testing();
});

#endif
