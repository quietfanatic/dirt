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
     // Domain-specific numeric error code if applicable.  The interpretation of
     // this depends on code (the string one).  For IO errors, this will be
     // errno.
    i64 number = 0;
     // Extra information in name: value format
    UniqueArray<std::pair<SharedString, SharedString>> tags;
     // If this wrapped a different error, this stores it.  code will be
     // e_External and details will have the CPP type (hopefully demangled) and
     // the what() of the error.
    std::exception_ptr external;
     // A lot of exception handling stuff assumes that the string returned by
     // what() will last a while, so store it here.
    mutable UniqueString what_cache;
     // Construction
    [[gnu::cold]]
    Error () noexcept = default;
    [[gnu::cold]]
    Error (ErrorCode c, SharedString&& d, i64 n = 0) noexcept;
    [[gnu::cold]]
    Error (std::exception_ptr&& e) noexcept;
     // Error is copyable and movable, but it generally results in smaller code
     // to directly throw Error(...), then catch it to add tags and rethrow.
    [[gnu::cold]]
    Error (const Error&) noexcept;
    [[gnu::cold]]
    Error (Error&&) noexcept = default;
    [[gnu::cold]]
    ~Error ();
    [[gnu::cold]]
    const char* what () const noexcept override;

     // Returns the value of the tag, or "" if it doesn't exist.
    [[gnu::cold]]
    Str get_tag (Str name) noexcept;
     // Sets tag (replaces last occurrence if already exists)
    [[gnu::cold]]
    void set_tag (SharedString name, SharedString value) noexcept;
     // Adds the tag (doesn't check if it's already been added)
    [[gnu::cold]]
    void add_tag (SharedString name, SharedString value) noexcept;
     // set_tag then rethrow.  *this must be the current exception!
    [[noreturn, gnu::cold]]
    void rethrow_with_tag (SharedString name, SharedString value);
     // Lightweight variant
    [[noreturn, gnu::cold]]
    void rethrow_with_tag (StaticString name, const char* value);
};

 // Get current Error.  If handling a different sort of exception, wraps it in
 // an Error with .code = e_External and .external = the original exception.
[[gnu::cold]]
Error& current_error () noexcept;

 // Simple noinline wrapper around construct and throw to reduce code bloat
[[noreturn, gnu::cold]]
void raise (ErrorCode code, SharedString details);
[[noreturn, gnu::cold]]
void raise (ErrorCode code, SharedString details, i64 number);

///// MISC UTILITY

UniqueString show_source_location (std::source_location) noexcept;

 // Call this when an exception is thrown in a place where cleaning up is
 // impossible.
[[noreturn, gnu::cold]]
void unrecoverable_exception (Str when) noexcept;

///// GENERAL ERROR CODES

 // Unspecified error.  Use this for rare situations that you can't be bothered
 // to make a new error code for (be sure to write something descriptive in
 // details though).
constexpr ErrorCode e_General = "uni::e_General";
 // Someone else's error type, std::rethrow_exception(e.external) to unwrap
constexpr ErrorCode e_External = "uni::e_External";

///// INLINES

namespace in {

[[gnu::cold]]
void set_tag_impl (Error&, SharedString::Impl name, SharedString::Impl value) noexcept;
[[gnu::cold]]
void add_tag_impl (Error&, SharedString::Impl name, SharedString::Impl value) noexcept;
[[noreturn, gnu::cold]]
void rethrow_with_tag_impl (Error&, SharedString::Impl name, SharedString::Impl value);
[[noreturn, gnu::cold]] NOINLINE
void raise_impl (ErrorCode code, SharedString::Impl details);
[[noreturn, gnu::cold]] NOINLINE
void raise_impl (ErrorCode code, SharedString::Impl details, i64 number);

} // in

ALWAYS_INLINE
void Error::set_tag (SharedString name, SharedString value) noexcept {
    auto n = name.impl; name.impl = {};
    auto v = value.impl; value.impl = {};
    in::set_tag_impl(*this, n, v);
}
ALWAYS_INLINE
void Error::add_tag (SharedString name, SharedString value) noexcept {
    auto n = name.impl; name.impl = {};
    auto v = value.impl; value.impl = {};
    in::add_tag_impl(*this, n, v);
}
ALWAYS_INLINE
void Error::rethrow_with_tag (SharedString name, SharedString value) {
    auto n = name.impl; name.impl = {};
    auto v = value.impl; value.impl = {};
    in::rethrow_with_tag_impl(*this, n, v);
}

ALWAYS_INLINE
void raise (ErrorCode code, SharedString details) {
    auto impl = details.impl;
    details.impl = {};
    in::raise_impl(code, impl);
}

ALWAYS_INLINE
void raise (ErrorCode code, SharedString details, i64 number) {
    auto impl = details.impl;
    details.impl = {};
    in::raise_impl(code, impl, number);
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
