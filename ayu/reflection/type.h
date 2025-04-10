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
 //
 // Types can be constructed at constexpr time, but their readonly bit cannot be
 // set or read.
struct Type {
     // Uses a tagged pointer; the first bit determines readonly (const), and the rest
     // points to an ayu::in::Description.
    union {
        usize data;
        const in::TypeInfo* type_info; // Write-only
    };

    constexpr Type () : data(0) { }
     // Construct from internal data
    constexpr Type (const in::TypeInfo* ti) : type_info(ti) { }
    Type (const in::TypeInfo* ti, bool readonly) :
        data(reinterpret_cast<usize>(ti) | readonly)
    { }
     // Never throws.  Strips reference info
    template <class T> requires (
        !std::is_volatile_v<std::remove_reference_t<T>>
    ) static constexpr
    Type CppType () noexcept {
        if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
            return Type(
                in::get_type_info<
                    std::remove_const_t<std::remove_reference_t<T>>
                >(),
                true
            );
        }
        else return Type(
            in::get_type_info<
                std::remove_const_t<std::remove_reference_t<T>>
            >()
        );
    }
     // Can throw TypeNotFound
    Type (Str name, bool readonly = false) :
        Type(in::require_type_for_name(name), readonly)
    { }

     // Checks if this is the empty type.
    explicit constexpr operator bool () const { return data & ~1; }
     // Checks if this type is readonly (const).
    bool readonly () const { return data & 1; }
     // Add or remove readonly bit
    Type add_readonly () const { return Type(get_info(), true); }
    Type remove_readonly () const { return Type(get_info(), false); }

     // Get human-readable type name (whatever name was registered with
     // AYU_DESCRIBE).  This ignores the readonly bit.
    StaticString name () const {
        return in::get_type_name(get_info());
    }
     // Get the sizeof() of this type
    constexpr usize cpp_size () const {
        return get_description()->cpp_size;
    }
     // Get the alignof() of this type
    constexpr usize cpp_align () const {
        return get_description()->cpp_align;
    }
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
        return (T*)try_upcast_to(Type::CppType<T>(), p);
    }
    Mu* upcast_to (Type, Mu*) const;
    template <class T>
    T* upcast_to (Mu* p) const {
        return (T*)upcast_to(Type::CppType<T>(), p);
    }

     // Internal
    constexpr in::TypeInfo* get_info () const {
        return reinterpret_cast<in::TypeInfo*>(data & ~1);
    }
    constexpr const in::Description* get_description () const {
        return get_info()->description;
    }
};

 // The same type will always have the same description pointer.
constexpr bool operator == (Type a, Type b) {
    return a.data == b.data;
}
constexpr bool operator != (Type a, Type b) {
    return a.data != b.data;
}
constexpr bool operator < (Type a, Type b) {
    return a.data < b.data;
}

 // Tried to map a C++ type to an AYU type, but AYU doesn't know about that type
 // (it has no AYU_DESCRIBE description).
constexpr ErrorCode e_TypeUnknown = "ayu::e_TypeUnknown";
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

