 // This is a lightweight error type that can provide all necessary information
 // without bloating the binary size with lots of rarely-used RTTI,
 // constructors, and destructors.

#pragma once

#include <exception>

#include "arrays.h"
#include "common.h"
#include "copy-ref.h"

namespace uni {

using ErrorCode = const char*;

 // Class for ayu-related errors.
struct Error : std::exception {
     // An API-stable constant string.  Assigned values will be in the
     // associated header files.  TODO: add subcode, for errno and other error
     // codes.
    ErrorCode code = null;
     // More information about the error, subject to change.
    AnyString details;
     // Extra information in name: value format
    UniqueArray<std::pair<AnyString, AnyString>> tags;
     // If this wrapped a different error, this stores it.  code will be
     // e_External and details will have the CPP type (hopefully demangled) and
     // the what() of the error.
    std::exception_ptr external;
     // A lot of exception handling stuff assumes that the string returned by
     // what() will last a while, so store it here.
    mutable UniqueString what_cache;
    [[gnu::cold]] ~Error ();
    const char* what () const noexcept override;

     // Returns the value of the tag, or "" if it doesn't exist.
    const AnyString& get_tag (const AnyString& name);
     // Adds the tag (doesn't check if it's already been added)
    void add_tag (AnyString name, AnyString value);
     // If you want to prevent duplicate tags, do
     //     if (!e.get_tag("foo")) {
     //         e.add_tag("foo", cat("glarch ", barch, " parch"));
     //     }
};

 // Simple noinline wrapper around construct and throw to reduce code bloat
[[noreturn, gnu::cold]] NOINLINE
void raise (ErrorCode code, MoveRef<AnyString> details);

 // Unspecified error
constexpr ErrorCode e_General = "uni::e_General";
 // Someone else's error type, std::rethrow(e.external) to unwrap
constexpr ErrorCode e_External = "uni::e_External";

 // Call this when an exception is thrown in a place where cleaning up is
 // impossible.
[[noreturn]] void unrecoverable_exception (Str when) noexcept;

} // uni

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

namespace uni {
    template <const ErrorCode& ec, class F>
    bool throws_code (F cb, std::string_view name = "") {
        return tap::throws_check<Error>(
            std::forward<F>(cb),
            [](const Error& e){ return Str(e.code) == Str(ec); },
            name
        );
    }
}

#endif
