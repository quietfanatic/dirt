// This module contains various types and exceptions that are used throughout
// the library.

#pragma once

#include <cstdint>
#include <cwchar>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include "../uni/arrays.h"
#include "../uni/assertions.h"
#include "../uni/callback-ref.h"
#include "../uni/common.h"
#include "../uni/copy-ref.h"
#include "../uni/strings.h"

namespace iri { struct IRI; }

namespace ayu {
using namespace uni;
using iri::IRI;

///// BASIC TYPES AND STUFF

struct Document; // document.h
struct Dynamic; // dynamic.h
struct Error; // error.h
struct Location; // location.h
using LocationRef = CopyRef<Location>;
struct Pointer; // pointer.h
struct Reference; // reference.h
struct Resource; // resource.h
struct Tree; // tree.h
using TreeRef = CRef<Tree, 16>;
struct Type; // type.h

using TreeArray = SharedArray<Tree>;
using TreeArraySlice = Slice<Tree>;
using TreePair = std::pair<AnyString, Tree>;
using TreeObject = SharedArray<TreePair>;
using TreeObjectSlice = Slice<TreePair>;

 // Unknown type that will never be defined.  This has a similar role to void,
 // except:
 //   - You can have a reference Mu& or Mu&&.
 //   - A pointer or reference to Mu is always supposed to refer to a
 //     constructed item, not an unconstructed buffer.  Functions that take or
 //     return unconstructed or untyped buffers use void* instead.
 //   - This does not track constness (in general there shouldn't be any
 //     const Mu&).
struct Mu;

///// UTILITY

void dump_refs (Slice<Reference>);
 // Primarily for debugging.  Prints item_to_string(Reference(&v)) to stderr
template <class... Args>
void dump (const Args&... v) {
    dump_refs({&v...});
}

///// EXCEPTIONS

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
void raise (ErrorCode code, UniqueString&& details);

 // Unspecified error
constexpr ErrorCode e_General = "General";
 // Non-AYU error, std::rethrow(e.external) to unwrap
constexpr ErrorCode e_External = "External";
 // TODO: Move these into an IO module
constexpr ErrorCode e_OpenFailed = "OpenFailed";
constexpr ErrorCode e_ReadFailed = "ReadFailed";
constexpr ErrorCode e_WriteFailed = "WriteFailed";
constexpr ErrorCode e_CloseFailed = "CloseFailed";

 // Called when an exception is thrown in a place where the library can't
 // properly clean up after itself, such as when a resource value throws
 // from its destructor.
[[noreturn]] void unrecoverable_exception (Str when) noexcept;

} // namespace ayu

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

namespace ayu {
    template <const ErrorCode& ec>
    bool throws_code (tap::CallbackRef<void()> cb, std::string_view name = "") {
        return tap::throws_check<Error>(
            cb, [](const Error& e){ return Str(e.code) == Str(ec); }, name
        );
    }
}

#endif
