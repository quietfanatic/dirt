#include "errors.h"

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

#include "io.h"

namespace uni {

const char* Error::what () const noexcept {
    what_cache = cat(code, "; ", details);
    if (tags) {
        encat(what_cache, Caterator("", tags.size(), [&](usize i){
            return cat("\n    {", tags[i].first, ": ", tags[i].second, '}');
        }));
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

[[gnu::cold, maybe_unused]] static
auto get_demangled_name (const std::type_info& t) noexcept {
#if __has_include(<cxxabi.h>)
    int status;
    char* demangled = abi::__cxa_demangle(t.name(), nullptr, nullptr, &status);
    if (status != 0) return cat("?(Failed to demangle ", t.name(), ')');
    auto r = UniqueString(demangled);
    free(demangled);
    return r;
#else
     // Probably MSVC, which automatically demangles.
    return Str(t.name());
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
            get_demangled_name(typeid(e)), ": ", e.what()
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
