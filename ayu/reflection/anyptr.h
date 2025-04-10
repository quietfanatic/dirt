// An ayu::AnyPtr is a runtime-typed pointer.  It is trivially copyable and
// destructable, and can be casted from and to native pointers.
//
// AnyPtrs cannot be constructed until main() starts (except for the typeless
// empty AnyPtr).
#pragma once
#include "../../uni/hash.h"
#include "type.h"

namespace ayu {

struct AnyPtr {
     // Usually putting metadata before data is faster but for some reason it
     // appears to work better this way
    Mu* address;
    Type type;

    constexpr AnyPtr (Null n = null) : address(n) { }
    constexpr AnyPtr (Type t, Mu* a) : address(a), type(t) { }

     // Coercion from pointer is explicit for AnyPtr* and AnyRef* to avoid
     // mistakes.  Watch out for when you're working with template parameters!
    template <class T> requires (
        !std::is_same_v<std::remove_cv_t<T>, void> &&
        !std::is_same_v<std::remove_cv_t<T>, Mu>
    ) explicit (
        std::is_same_v<std::remove_cv_t<T>, AnyPtr> ||
        std::is_same_v<std::remove_cv_t<T>, AnyRef>
    ) AnyPtr (T* a) : address((Mu*)a), type(Type::For<T>()) { }

     // Returns false if this AnyPtr is either (typed) null or (typeless)
     // empty.
    constexpr explicit operator bool () const { return address; }
     // Returns true only for the typeless empty AnyPtr.
    constexpr bool empty () const { return !!type; }

    bool readonly () const { return type.readonly(); }
    AnyPtr add_readonly () const {
        return AnyPtr(type.add_readonly(), address);
    }
    AnyPtr remove_readonly () const {
        return AnyPtr(type.remove_readonly(), address);
    }

    AnyPtr try_upcast_to (Type t) const {
        return AnyPtr(t, type.try_upcast_to(t, address));
    }
    template <class T>
    T* try_upcast_to () const {
        return type.try_upcast_to<T>(address);
    }

    AnyPtr upcast_to (Type t) const {
        return AnyPtr(t, type.upcast_to(t, address));
    }
    template <class T>
    T* upcast_to () const {
        return type.upcast_to<T>(address);
    }

    template <class T>
    T* expect_exact () const {
        expect(type == Type::For<T>());
        return reinterpret_cast<T*>(address);
    }

    template <class T>
        requires (!std::is_same_v<std::remove_cv_t<T>, void>
               && !std::is_same_v<std::remove_cv_t<T>, Mu>)
    operator T* () const { return type.upcast_to<T>(address); }
};

 // AnyPtrs have a slightly evil property where a readonly pointer can equal a
 // non-readonly pointer.  This may be unintuitive, but it matches the behavior
 // of native C++ pointers and also makes looking them up in a hash table much
 // easier.
constexpr bool operator == (const AnyPtr& a, const AnyPtr& b) {
    return a.address == b.address &&
        a.type.remove_readonly() == b.type.remove_readonly();
}
constexpr bool operator != (const AnyPtr& a, const AnyPtr& b) {
    return !(a == b);
}
constexpr bool operator < (const AnyPtr& a, const AnyPtr& b) {
    return a.address == b.address
        ? a.type.remove_readonly() < b.type.remove_readonly()
        : a.address < b.address;
}

} // namespace ayu

template <>
struct std::hash<ayu::AnyPtr> {
    std::size_t operator () (const ayu::AnyPtr& p) const {
        return uni::hash_combine(
            std::hash<ayu::Mu*>{}(p.address),
            std::hash<ayu::Type>{}(p.type.remove_readonly())
        );
    }
};
