#pragma once
#include <functional>  // for std::hash
#include "../common.internal.h"
#include "type.internal.h"

namespace ayu {

 // Represents a type known to ayu.  Provides dynamically-typed construction and
 // destruction for any type as long as it has an AYU_DESCRIBE declaration.
 // Can represent const types (called readonly in AYU), but not reference or
 // volatile types.
 //
 // The default value will cause null derefs if you do anything with it.
struct Type {
     // Uses a tagged pointer; the first bit determines readonly (const), and the rest
     // points to an ayu::in::DescriptionHeader.
    union {
        usize data;
        const void* ptr; // For constexpr writes.
    };

     // Construct empty type
    constexpr Type () : ptr(null) { }
     // Construct from C++ type.  Never throws.  Strips reference info.
    template <class T> requires (
        !std::is_volatile_v<std::remove_reference_t<T>>
    ) static
    Type For () noexcept;
     // Construct from name.  Can throw TypeNotFound
    Type (Str name, bool readonly = false);

     // Checks if this is the empty type.
    explicit constexpr operator bool () const { return data & ~1; }
     // Checks if this type is readonly (const).
    bool readonly () const { return data & 1; }
     // Add or remove readonly bit
    Type add_readonly () const { return Type(data | 1); }
    Type remove_readonly () const { return Type(data & ~1); }

     // Get human-readable type name (whatever name was registered with
     // AYU_DESCRIBE).  This ignores the readonly bit.  Returns "" for the empty
     // type.
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
     // and recurses through those accessors.  Note also only information
     // provided through AYU_DESCRIBE will be used; C++'s native inheritance
     // system has no influence.
     //
     // try_upcast_to will return null if the requested base class was not found
     // in the derived class's inheritance hierarchy, or if the address of the
     // base class can't be retrieved (goes through value_funcs or some such).
     // upcast_to will throw CannotCoerce (unless given null, in which case
     // it will return null).
     //
     // Finally, casting from non-readonly to readonly types is allowed, but not
     // vice versa.
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

     // Same as Type::For, but constexpr.  Only works with non-const types, and
     // some uses of this will cause weird errors like "specialization of ...
     // after instantiation" or "incomplete type ... used in nested name
     // specifier".  And what's worse, these errors may only pop up during
     // optimized builds.
     //
     // I believe it is safe to use this in templated contexts or partially
     // specialized contexts, but not in non-template or fully specialized
     // contexts.  It may also need to be dependent on a template parameter.
     //
     // It is also safe to use this in a translation unit that doesn't have any
     // AYU_DESCRIBE blocks.
     //
     // For maximum safety, always use Type::For unless you absolutely need it
     // to be constexpr, and if you do use this, test with optimizations enabled
     // (-O1 is enough).
    template <class T> requires (
        !std::is_volatile_v<std::remove_reference_t<T>>
        && !std::is_const_v<std::remove_reference_t<T>>
    ) static constexpr
    Type For_constexpr () noexcept;

    const in::DescriptionPrivate* description () const {
        return reinterpret_cast<const in::DescriptionPrivate*>(data & ~1);
    }
    Type (const in::DescriptionHeader* ptr, bool readonly = false) :
        data(reinterpret_cast<usize>(ptr) | readonly)
    { }
     // To avoid the const void* overload from applying to string literals
    template <usize n>
    Type (const char(& s )[n]) : Type(Str(s)) { }
     // Construct from internal data.  Private because they're just too
     // dangerous.  void* is the only thing you can reinterpret_cast to at
     // compile time.
    private:
    constexpr explicit Type (usize data) : data(data) { }
    constexpr explicit Type (const void* ptr) : ptr(ptr) { }
};

 // The same type will always have the same description pointer.  Compare ptr
 // instead of data so that this can be constexpr (only the ptr variant can be
 // written at constexpr time).
constexpr bool operator == (Type a, Type b) {
    return a.ptr == b.ptr;
}
constexpr bool operator != (Type a, Type b) {
    return a.ptr != b.ptr;
}
constexpr bool operator < (Type a, Type b) {
    return a.ptr < b.ptr;
}

 // Tried to look up a type be name but there is no registered type with that
 // name.
constexpr ErrorCode e_TypeNotFound = "ayu::e_TypeNotFound";
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
        return hash<void*>()((void*)t.data);
    }
};

 // This is just too long to put up front
template <class T> requires (
    !std::is_volatile_v<std::remove_reference_t<T>>
)
ayu::Type ayu::Type::For () noexcept {
    return Type(
        reinterpret_cast<const in::DescriptionHeader*>(
            &AYU_Describe<std::remove_cvref_t<T>>::AYU_description
        ),
        std::is_const_v<std::remove_reference_t<T>>
    );
}
template <class T> requires (
    !std::is_volatile_v<std::remove_reference_t<T>>
    && !std::is_const_v<std::remove_reference_t<T>>
) constexpr
ayu::Type ayu::Type::For_constexpr () noexcept {
    return Type(
        &AYU_Describe<std::remove_cvref_t<T>>::AYU_description
    );
}
