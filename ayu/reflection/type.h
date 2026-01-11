#pragma once
#include <functional>  // for std::hash
#include "../common.internal.h"
#include "type.internal.h"

namespace ayu {

 // Represents a type known to ayu.  Provides dynamically-typed construction and
 // destruction for any type as long as it has an AYU_DESCRIBE declaration.
 // Cannot represent const, volatile, references, or void at the top level, but
 // can represent pointers to those (well, not pointers to references, which C++
 // itself bans).
 //
 // Types can sometimes be constructed at constexpr time, and sometimes not,
 // depending on some obscure C++ language rules.  No operations on Types can be
 // done at constexpr time; but if you want to manipulate types at constexpr
 // time, you can just use C++ types.
 //
 // There is an empty Type, which will cause null derefs if you do anything but
 // boolify it.
struct Type {
     // Descriptions have incomplete final types, so the only thing you can cast
     // their pointers to is void*.
    const void* data;

    ///// CONSTRUCTION

     // Construct empty Type
    constexpr Type () : data(null) { }

     // Construct from internal data (no accidental coercions to void*)
    template <class T> requires (
        std::is_void_v<T> || std::is_same_v<T, in::DescriptionPrivate>
    )
    constexpr explicit Type (const T* p) : data(p) { }

     // Construct from name.  Can throw TypeNameNotFound.
    explicit Type (Str name) : data(in::require_type_with_name(name)) { }

     // Construct from C++ type.  Never throws.
    template <Describable T> static
    Type For () noexcept {
        return Type((const void*)&AYU_Describe<T>::AYU_description);
    }

    ///// INFO

     // Checks if this is the empty type.
    explicit constexpr operator bool () const { return data; }

     // Get human-readable type name (whatever name was registered with
     // AYU_DESCRIBE).
    StaticString name () const noexcept {
        return reinterpret_cast<const in::DescriptionHeader*>(data)->get_name();
    }

     // Get the sizeof() this type
    usize cpp_size () const {
        return reinterpret_cast<const in::DescriptionHeader*>(data)->cpp_size;
    }
     // Get the alignof() this type
    usize cpp_align () const {
        return reinterpret_cast<const in::DescriptionHeader*>(data)->cpp_align;
    }

    ///// MISC

     // Same as Type::For, but constexpr.  Some uses of this will cause weird
     // errors like "specialization of ...  after instantiation" or "incomplete
     // type ... used in nested name specifier".  And what's worse, these errors
     // may only pop up during optimized builds.
     //
     // I believe it is safe to use this on a "dependent type".  See
     // https://en.cppreference.com/w/cpp/language/dependent_name
     //
     // It is also safe to use this in a translation unit that doesn't have any
     // AYU_DESCRIBE blocks.
     //
     // For maximum safety, always use Type::For unless you absolutely need it
     // to be constexpr, and if you do use this, test with optimizations enabled
     // (-O1 is enough).
    template <Describable T> static constexpr
    Type For_constexpr () noexcept {
        return Type((const void*)&AYU_Describe<T>::AYU_description);
    }
};

 // The same type will always have the same description pointer.
constexpr bool operator == (Type a, Type b) { return a.data == b.data; }
constexpr auto operator <=> (Type a, Type b) { return a.data <=> b.data; }

///// DYNAMICALLY TYPED OPERATIONS

 // Allocate a buffer appropriate for containing an instance of this type.
 // This uses operator new(size, align, nothrow), so either use
 // type.deallocate(p) or operator delete(p, align) to delete the pointer.
inline
void* dynamic_allocate (Type t) noexcept {
    return operator new(
        t.cpp_size(), std::align_val_t(t.cpp_align()), std::nothrow
    );
}

 // Deallocate a buffer previously allocated with allocate()
inline
void dynamic_deallocate (Type t, void* p) noexcept {
    operator delete(p, t.cpp_size(), std::align_val_t(t.cpp_align()));
}

 // Construct an instance of this type in-place.  Doesn't check that the target
 // location has the required size and alignment.  May throw
 // CannotDefaultConstruct or CannotDestruct.
void dynamic_default_construct (Type, void*);

 // Like dynamic_default_construct but skips the destructor check, so only use
 // it if you plan to leak the object forever or destroy it in some other
 // manner.
void dynamic_default_construct_without_destructor (Type, void*);

 // Destroy an instance of this type in-place.  The memory will not be
 // deallocated.  May throw CannotDestroy.
void dynamic_destroy (Type, Mu*);

 // Allocate and construct an instance of this type.
Mu* dynamic_default_new (Type);

 // Destruct and deallocate and instance of this type.
void dynamic_delete (Type, Mu*);

 // Cast from derived class to base class.  Does a depth-first search through
 // the derived class's description looking for accessors like:
 //  - delegate(...)
 //  - attr("name", ..., include)
 //  - elem(..., include)
 // and recurses through those accessors.  Note also that only information
 // provided through AYU_DESCRIBE will be used; C++'s native inheritance system
 // has no influence.
 //
 // Returns null if the requested base class was not found in the
 // derived class's inheritance hierarchy, or if the address of the base class
 // can't be retrieved (goes through value_funcs or some such).
Mu* dynamic_try_upcast (Type from, Type to, Mu*);

 // Throws TypeCantCast (unless given null, in which case it will return null).
Mu* dynamic_upcast (Type from, Type to, Mu*);

 // Previous versions of this library also had downcast_to (and cast_to, which
 // tries upcast then downcast) but it was dragging down refactoring and I
 // didn't end up actually using it.

 // Tried to look up a type be name but there is no registered type with that
 // name.
constexpr ErrorCode e_TypeNameNotFound = "ayu::e_TypeNameNotFound";
 // Tried to default construct a type that has no default constructor.
constexpr ErrorCode e_TypeCantDefaultConstruct = "ayu::e_TypeCantDefaultConstruct";
 // Tried to construct or destroy a type that has no destructor.
constexpr ErrorCode e_TypeCantDestroy = "ayu::e_TypeCantDestroy";
 // Tried to cast between types that can't be casted between.
constexpr ErrorCode e_TypeCantCast = "ayu::e_TypeCantCast";

} // namespace ayu

 // Allow hashing Type for std::unordered_map
template <>
struct std::hash<ayu::Type> {
    std::size_t operator () (ayu::Type t) const {
        return hash<const void*>()(t.data);
    }
};

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

template <>
struct tap::Show<ayu::Type> {
    uni::UniqueString show (ayu::Type v) { return v.name(); }
};

#endif
