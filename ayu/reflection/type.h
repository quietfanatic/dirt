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
    const void* data;

     // Construct empty Type
    constexpr Type () : data(null) { }

     // Construct from C++ type.  Never throws.
    template <class T> requires (Describable<T>) static
    Type For () noexcept;

     // Construct from name.  Can throw TypeNameNotFound.
    Type (Str name);

     // Checks if this is the empty type.
    explicit constexpr operator bool () const { return data; }
     // Get human-readable type name (whatever name was registered with
     // AYU_DESCRIBE).  Returns "" for the empty type.
    StaticString name () const noexcept;
     // Get the sizeof() of this type
    usize cpp_size () const;
     // Get the alignof() of this type
    usize cpp_align () const;
     // Construct an instance of this type in-place.  The target must have at
     // least the required size and alignment.  May throw CannotDefaultConstruct
     // or CannotDestroy.
    void default_construct (void* target) const;
     // Destory an instance of this type in-place.  The memory will not be
     // deallocated.
    void destroy (Mu*) const;
     // Allocate a buffer appropriate for containing an instance of this type.
     // This uses operator new(size, align, nothrow), so either use
     // type.deallocate(p) or operator delete(p, align) to delete the pointer.
    void* allocate () const noexcept;
     // Deallocate a buffer previously allocated with allocate()
    void deallocate (void*) const noexcept;
     // Allocate and construct an instance of this type.
    Mu* default_new () const;
     // Destruct and deallocate and instance of this type.
     // Should be called delete, but, you know
    void delete_ (Mu*) const;

     // Cast from derived class to base class.  Does a depth-first search
     // through the derived class's description looking for accessors like:
     //  - delegate(...)
     //  - attr("name", ..., include)
     //  - elem(..., include)
     // and recurses through those accessors.  Note also that only information
     // provided through AYU_DESCRIBE will be used; C++'s native inheritance
     // system has no influence.
     //
     // try_upcast_to will return null if the requested base class was not found
     // in the derived class's inheritance hierarchy, or if the address of the
     // base class can't be retrieved (goes through value_funcs or some such).
     // upcast_to will throw TypeCantCast (unless given null, in which case it
     // will return null).
     //
     // Previous versions of this library also had downcast_to (and cast_to,
     // which tries upcast then downcast) but it was dragging down refactoring
     // and I didn't end up actually using it.
    Mu* try_upcast_to (Type, Mu*) const;
    template <class T>
    T* try_upcast_to (Mu* p) const {
        return (T*)try_upcast_to(Type::For<T>(), p);
    }
    Mu* upcast_to (Type, Mu*) const;
    template <class T>
    T* upcast_to (Mu* p) const {
        return (T*)upcast_to(Type::For<T>(), p);
    }

     ///// INTERNAL

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
    template <class T> requires (Describable<T>) static constexpr
    Type For_constexpr () noexcept;

     // TODO: put this back on DescriptionPrivate
    const in::DescriptionPrivate* description () const {
        return reinterpret_cast<const in::DescriptionPrivate*>(data);
    }
     // TODO: check if this is still used
    Type (const in::DescriptionHeader* ptr) : data(ptr) { }

     // To avoid the const void* overload from applying to string literals
    template <usize n>
    Type (const char(& s )[n]) : Type(Str(s)) { }

    private:
    friend struct AnyPtr; // TODO: make this better
    constexpr explicit Type (const void* ptr) : data(ptr) { }
};

 // The same type will always have the same description pointer.  Compare ptr
 // instead of data so that this can be constexpr (only the ptr variant can be
 // written at constexpr time).
constexpr bool operator == (Type a, Type b) { return a.data == b.data; }
constexpr auto operator <=> (Type a, Type b) { return a.data <=> b.data; }

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

template <class T> requires (ayu::Describable<T>)
ayu::Type ayu::Type::For () noexcept {
    return Type((const void*)&AYU_Describe<T>::AYU_description);
}
template <class T> requires (ayu::Describable<T>) constexpr
ayu::Type ayu::Type::For_constexpr () noexcept {
    return Type((const void*)&AYU_Describe<T>::AYU_description);
}

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

template <>
struct tap::Show<ayu::Type> {
    uni::UniqueString show (ayu::Type v) { return v.name(); }
};

#endif
