// An ayu::Pointer is a runtime-typed pointer.  It is trivially copyable and
// destructable, and can be casted from and to native pointers.
//
// Pointers cannot be constructed until main() starts (except for the typeless
// empty Pointer).

#pragma once
#include "../../uni/hash.h"
#include "type.h"

namespace ayu {

struct Pointer {
    Mu* address;
    Type type;

    constexpr Pointer (Null n = null) : address(n) { }
    constexpr Pointer (Type t, Mu* a) : address(a), type(t) { }

    template <class T> requires (
        !std::is_same_v<std::remove_cv_t<T>, void> &&
        !std::is_same_v<std::remove_cv_t<T>, Mu>
    ) explicit (
        std::is_same_v<std::remove_cv_t<T>, Pointer> ||
        std::is_same_v<std::remove_cv_t<T>, Reference>
    ) Pointer (T* a) : address((Mu*)a), type(Type::CppType<T>()) { }

     // Returns false if this Pointer is either (typed) null or (typeless)
     // empty.
    constexpr explicit operator bool () const { return address; }
     // Returns true only for the typeless empty Pointer.
    constexpr bool empty () const { return !!type; }

    constexpr bool readonly () const { return type.readonly(); }
    constexpr Pointer add_readonly () const {
        return Pointer(type.add_readonly(), address);
    }
    constexpr Pointer remove_readonly () const {
        return Pointer(type.remove_readonly(), address);
    }

    Pointer try_upcast_to (Type t) const {
        return Pointer(t, type.try_upcast_to(t, address));
    }
    template <class T>
    T* try_upcast_to () const {
        return type.try_upcast_to<T>(address);
    }

    Pointer upcast_to (Type t) const {
        return Pointer(t, type.upcast_to(t, address));
    }
    template <class T>
    T* upcast_to () const {
        return type.upcast_to<T>(address);
    }

    Pointer try_downcast_to (Type t) const {
        return Pointer(t, type.try_downcast_to(t, address));
    }
    template <class T>
    T* try_downcast_to () const {
        return type.try_downcast_to<T>(address);
    }

    Pointer downcast_to (Type t) const {
        return Pointer(t, type.downcast_to(t, address));
    }
    template <class T>
    T* downcast_to () const {
        return type.downcast_to<T>(address);
    }

    Pointer try_cast_to (Type t) const {
        return Pointer(t, type.try_cast_to(t, address));
    }
    template <class T>
    T* try_cast_to () const {
        return type.try_cast_to<T>(address);
    }

    Pointer cast_to (Type t) const {
        return Pointer(t, type.cast_to(t, address));
    }
    template <class T>
    T* cast_to () const {
        return type.cast_to<T>(address);
    }

    template <class T>
        requires (!std::is_same_v<std::remove_cv_t<T>, void>
               && !std::is_same_v<std::remove_cv_t<T>, Mu>)
    operator T* () const { return type.upcast_to<T>(address); }
};

 // Pointers have a slightly evil property where a readonly pointer can equal a
 // non-readonly pointer.  This may be unintuitive, but it matches the behavior
 // of native C++ pointers and also makes looking them up in a hash table much
 // easier.
constexpr bool operator == (const Pointer& a, const Pointer& b) {
    return a.address == b.address &&
        a.type.remove_readonly() == b.type.remove_readonly();
}
constexpr bool operator != (const Pointer& a, const Pointer& b) {
    return !(a == b);
}
constexpr bool operator < (const Pointer& a, const Pointer& b) {
    return a.address == b.address
        ? a.type.remove_readonly() < b.type.remove_readonly()
        : a.address < b.address;
}

} // namespace ayu

template <>
struct std::hash<ayu::Pointer> {
    std::size_t operator () (const ayu::Pointer& p) const {
        return uni::hash_combine(
            std::hash<ayu::Mu*>{}(p.address),
            std::hash<ayu::Type>{}(p.type.remove_readonly())
        );
    }
};
