 // This is a lightweight error type that can provide all necessary information
 // without bloating the binary size with lots of rarely-used RTTI,
 // constructors, and destructors.
 //
 // An error has:
 //     code: an api-safe static string
 //     details: a human-readable message (can be generated at runtime)
 //     tags: an optional list of name:value string pairs
 //     external: A std::exception_ptr for wrapping other error types.
 //
 // In addition, there's a raise(code, details) which wraps the throwing code,
 // because throw expressions can be surprisingly bulky, so this deduplicates
 // it.

#pragma once

#include <exception>

#include "arrays.h"
#include "common.h"
#include "copy-ref.h"

namespace uni {

///// ERROR CLASS

 // ErrorCodes are always compared stringwise, not by address.  It's tempting to
 // use const char* for this to faintly reduce compiled code size, but string
 // literals can't be compared by address (the compiler doesn't always
 // deduplicate them, even when assigned to constexpr variables), and I can't
 // reliably remember to use strcmp everywhere.
using ErrorCode = StaticString;

 // Class for ayu-related errors.
struct Error : std::exception {
     // An API-stable constant string.  Assigned values will be in the
     // associated header files.
    ErrorCode code;
     // More information about the error, subject to change.
    SharedString details;
     // Extra information in name: value format
    UniqueArray<std::pair<SharedString, SharedString>> tags;
     // If this wrapped a different error, this stores it.  code will be
     // e_External and details will have the CPP type (hopefully demangled) and
     // the what() of the error.
    std::exception_ptr external;
     // A lot of exception handling stuff assumes that the string returned by
     // what() will last a while, so store it here.
    mutable UniqueString what_cache;
    Error () noexcept = default;
    Error (const Error&) noexcept;
    ~Error ();
    const char* what () const noexcept override;

     // Returns the value of the tag, or "" if it doesn't exist.
    Str get_tag (Str name) noexcept;
     // Sets tag (replaces last occurrence if already exists)
    void set_tag (SharedString name, SharedString value) noexcept;
     // Adds the tag (doesn't check if it's already been added)
    void add_tag (SharedString name, SharedString value) noexcept;
     // If you want to prevent duplicate tags, do
     //     if (!e.get_tag("foo")) {
     //         e.add_tag("foo", cat("glarch ", barch, " parch"));
     //     }
};

 // Get current Error.  If handling a different sort of exception, wraps it in
 // an Error with .code = e_External and .external = the original exception.
Error& current_error () noexcept;

 // Simple noinline wrapper around construct and throw to reduce code bloat
[[noreturn]] void raise (ErrorCode code, SharedString details);

 // Call this when an exception is thrown in a place where cleaning up is
 // impossible.
[[noreturn]] void unrecoverable_exception (Str when) noexcept;

 // Probably useless without rtti
UniqueString demangle_cpp_name (const char* name) noexcept;

///// GENERAL ERROR CODES

 // Unspecified error
constexpr ErrorCode e_General = "uni::e_General";
 // Someone else's error type, std::rethrow_exception(e.external) to unwrap
constexpr ErrorCode e_External = "uni::e_External";

///// INLINES

namespace in {

[[noreturn, gnu::cold]] NOINLINE
void raise_impl (ErrorCode code, SharedString::Impl details);
void set_tag_impl (Error&, SharedString::Impl name, SharedString::Impl value) noexcept;
void add_tag_impl (Error&, SharedString::Impl name, SharedString::Impl value) noexcept;

} // in

[[gnu::cold]] ALWAYS_INLINE
void Error::set_tag (SharedString name, SharedString value) noexcept {
    auto n = name.impl; name.impl = {};
    auto v = value.impl; value.impl = {};
    in::set_tag_impl(*this, n, v);
}

[[gnu::cold]] ALWAYS_INLINE
void Error::add_tag (SharedString name, SharedString value) noexcept {
    auto n = name.impl; name.impl = {};
    auto v = value.impl; value.impl = {};
    in::add_tag_impl(*this, n, v);
}

[[gnu::cold]] ALWAYS_INLINE
void raise (ErrorCode code, SharedString details) {
    auto impl = details.impl;
    details.impl = {};
    in::raise_impl(code, impl);
}

} // uni

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

namespace uni {
    template <auto& ec, class F>
    bool throws_code (F cb, Str name = "") {
        return tap::throws_check<Error>(
            std::forward<F>(cb),
            [](const Error& e){ return e.code == ec; },
            name
        );
    }
}

#endif
