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
#include "../uni/errors.h"
#include "../uni/strings.h"

namespace iri { struct IRI; }

namespace ayu {
using namespace uni;
using iri::IRI;

///// BASIC TYPES AND STUFF

struct Document; // document.h
struct Dynamic; // dynamic.h
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

} // namespace ayu
