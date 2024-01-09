#include "errors.h"

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

#include "io.h"

namespace uni {

const char* Error::what () const noexcept {
    return (what_cache = cat(code, "; ", details)).c_str();
}
Error::~Error () { }

void raise (ErrorCode code, MoveRef<AnyString> details) {
    Error e;
    e.code = code;
    e.details = *move(details);
    throw e;
}

[[gnu::cold]] static
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
            get_demangled_name(typeid(e)), ": ", e.what()
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
