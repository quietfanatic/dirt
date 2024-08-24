// This module contains various types and exceptions that are used throughout
// the library.

#pragma once

#include <cstring>
#include "../uni/arrays.h"
#include "../uni/assertions.h"
#include "../uni/callback-ref.h"
#include "../uni/common.h"
#include "../uni/copy-ref.h"
#include "../uni/errors.h"
#include "../uni/strings.h"

namespace iri { struct IRI; }

namespace ayu {
using namespace uni;
using iri::IRI;

///// BASIC TYPES AND STUFF

 // resources/document.h
struct Document;

 // resources/resource.h
struct Resource;
struct SharedResource;
struct ResourceRef;

 // traversal/location.h
struct Location;
struct SharedLocation;
struct LocationRef;

 // reflection/type.h
struct Type;

 // reflection/anyptr.h
struct AnyPtr;

 // reflection/anyref.h
struct AnyRef;

 // reflection/anyval.h
struct AnyVal;

 // data/tree.h
struct Tree;
using TreeRef = CRef<Tree, 16>;
 // Since GCC 12.0, std::pair has extra concept shenanigans that cause weird
 // complicated errors when used with ArrayInterface, so we're throwing it in
 // the trash.
//using TreePair = std::pair<AnyString, Tree>;
template <class A, class B>
struct Pair {
    A first;
    B second;
};
using TreePair = Pair<AnyString, Tree>;

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

void dump_refs (Slice<AnyRef>);
 // Primarily for debugging.  Prints item_to_string(AnyRef(&v)) to stderr
template <class... Args>
void dump (const Args&... v) {
    dump_refs({&v...});
}

} // namespace ayu
