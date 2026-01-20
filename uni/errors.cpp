#include "errors.h"

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

#include "io.h"

namespace uni {

const char* Error::what () const noexcept {
    usize len = code.size() + 2 + details.size();
    for (usize i = 0; i < tags.size(); i++) {
        len += 5 + tags[i].first.size() + 2 + tags[i].second.size();
    }
    what_cache = UniqueString(Capacity(len));
    what_cache.append_expect_capacity(code);
    what_cache.append_expect_capacity("; ");
    what_cache.append_expect_capacity(details);
    for (usize i = 0; i < tags.size(); i++) {
        what_cache.append_expect_capacity("\n    ");
        what_cache.append_expect_capacity(tags[i].first);
        what_cache.append_expect_capacity(": ");
        what_cache.append_expect_capacity(tags[i].second);
    }
    return what_cache.c_str();
}
Error::~Error () { }

const AnyString& Error::get_tag (const AnyString& name) {
    for (auto& [n, v] : tags) {
        if (n.data() == name.data() || n == name) return v;
    }
    static constexpr AnyString empty = "";
    return empty;
}
void Error::add_tag (AnyString name, AnyString value) {
    tags.emplace_back(move(name), move(value));
}

void raise_inner (StaticString code, AnyString::Impl details) {
    Error e;
    e.code = code;
    e.details.impl = details;
    throw e;
}

UniqueString demangle_cpp_name (const char* name) noexcept {
#if __has_include(<cxxabi.h>)
    int status;
    char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    if (status != 0) return cat("!(Failed to demangle ", name, ')');
    auto r = UniqueString(demangled);
    free(demangled);
    return r;
#else
     // Probably MSVC, which automatically demangles.
    return name;
#endif
}

[[gnu::cold]]
void unrecoverable_exception (Str when) noexcept {
    try {
        throw std::current_exception();
    } catch (std::exception& e) {
        warn_utf8(cat(
            "ERROR: Unrecoverable exception ", when, ":\n    ",
#if defined(__GXX_RTTI) || defined(_CPPRTTI)
            demangle_cpp_name(typeid(e).name()), ": ", e.what()
#else
            "(Unknown type name): ", e.what()
#endif
        ));
        std::terminate();
    } catch (...) {
        warn_utf8(cat(
            "ERROR: Unrecoverable exception ", "of non-standard type ", when
        ));
        std::terminate();
    }
}

} // uni
