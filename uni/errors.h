 // This is a lightweight error type that can provide all necessary information
 // without bloating the binary size with lots of rarely-used RTTI,
 // constructors, and destructors.

#pragma once

#include <exception>

#include "common.h"
#include "copy-ref.h"
#include "strings.h"

namespace uni {

using ErrorCode = const char*;

 // Class for ayu-related errors.
struct Error : std::exception {
     // An API-stable constant string.  Assigned values will be in the
     // associated header files.
    ErrorCode code = null;
     // More information about the error, subject to change.
    AnyString details;
     // If this wrapped a different error, this stores it.  code will be
     // e_External and details will have the CPP type (hopefully demangled) and
     // the what() of the error.
    std::exception_ptr external;
     // A lot of exception handling stuff assumes that the string returned by
     // what() will never run out of lifetime, so store it here.
    mutable UniqueString what_cache;
     // Keep track of whether a traversal location has been added to details.
    bool has_travloc = false;
    [[gnu::cold]] ~Error ();
    const char* what () const noexcept override;
};

 // Simple noinline wrapper around construct and throw to reduce code bloat
[[noreturn, gnu::cold]] NOINLINE
void raise (ErrorCode code, MoveRef<UniqueString> details);

 // Unspecified error
constexpr ErrorCode e_General = "uni::e_General";
 // Non-AYU error, std::rethrow(e.external) to unwrap
constexpr ErrorCode e_External = "uni::e_External";

 // Call this when an exception is thrown in a place where cleaning up is
 // impossible.
[[noreturn]] void unrecoverable_exception (Str when) noexcept;

} // uni

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

namespace uni {
    template <const ErrorCode& ec>
    bool throws_code (tap::CallbackRef<void()> cb, std::string_view name = "") {
        return tap::throws_check<Error>(
            cb, [](const Error& e){ return Str(e.code) == Str(ec); }, name
        );
    }
}

#endif
