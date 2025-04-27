// This module contains various types and things that are used throughout the
// library.

#pragma once
#include "../uni/arrays.h"
#include "../uni/assertions.h"
#include "../uni/callback-ref.h"
#include "../uni/common.h"
#include "../uni/errors.h"
#include "../uni/strings.h"

///// PREDECLS

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

 // traversal/route.h
struct Route;
struct SharedRoute;
struct RouteRef;

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

 // Since GCC 12.0, std::pair has extra concept shenanigans that cause weird
 // complicated errors when used with ArrayInterface, so we're throwing it in
 // the trash.
//using TreePair = std::pair<AnyString, Tree>;
template <class A, class B>
struct Pair { A first; B second; };
using TreePair = Pair<AnyString, Tree>;

 // Unknown type that will never be defined.  This has a similar role to void,
 // except:
 //   - You can have a reference Mu& or Mu&&.
 //   - A pointer or reference to Mu is always supposed to refer to a
 //     constructed item, not an unconstructed buffer.  Functions that take or
 //     return unconstructed or untyped buffers use void* instead.
 //   - Mu does not track constness or volatility (in general there should never
 //     be a const Mu&).
struct Mu;

///// TYPE TRAITS / CONCEPTS

 // Type trait for the types that AYU can process at runtime.
 // There is no way to find out at compile time whether a type actually has been
 // described, because it could be described in another compilation unit, and
 // you'll only find out at link time.

template <class T> struct DescribableS { static constexpr bool value = true; };
 // No void or Mu (but void* is allowed)
template <> struct DescribableS<void> { static constexpr bool value = false; };
template <> struct DescribableS<Mu> { static constexpr bool value = false; };
 // No references
template <class T> struct DescribableS<T&> { static constexpr bool value = false; };
template <class T> struct DescribableS<T&&> { static constexpr bool value = false; };
 // No const or volatile (but pointers to them are allowed)
template <class T> struct DescribableS<const T> { static constexpr bool value = false; };
template <class T> struct DescribableS<volatile T> { static constexpr bool value = false; };
template <class T> struct DescribableS<const volatile T> { static constexpr bool value = false; };
 // No functions (but pointers to them are allowed)
template <class R, class... A> struct DescribableS<R(A...)> { static constexpr bool value = false; };

 // This is the actual type trait.
template <class T>
concept Describable = DescribableS<T>::value;

 // True if either T or const T is describable.
template <class T>
concept ConstableDescribable = Describable<std::remove_const_t<T>>;

 // Some basic traits
template <class T>
concept Destructible = requires (T v) { v.~T(); };

template <class T>
concept Movable = Destructible<T> && requires (T&& v) { T(move(v)); };

template <class T>
concept Copyable = Movable<T> && requires (const T& v) { T(v); };

 // Checks if the type is AnyPtr or AnyRef.  This prevents some dangerous
 // implicit coercions.
template <class T>
concept IsAnyPtrOrAnyRef = std::is_same_v<std::remove_cvref_t<T>, AnyPtr>
                        || std::is_same_v<std::remove_cvref_t<T>, AnyRef>;

 // Like std::is_base_of_v but also returns true if T and Base are the same
 // union type.
template <class T, class Base>
concept SameOrBase = std::is_same_v<T, Base> || std::is_base_of_v<T, Base>;

///// DEBUGGING

void dump_refs (Slice<AnyRef>);
 // Primarily for debugging.  Prints item_to_string(AnyRef(&v)) to stderr
template <class... Args>
void dump (const Args&... v) {
    dump_refs({&v...});
}

} // namespace ayu
