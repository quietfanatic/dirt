// An ayu::AnyPtr is a runtime-typed pointer.  It is trivially copyable and
// destructable, and can be casted from and to native pointers.
//
// AnyPtr can represent pointers-to-const (called readonly pointers) but not
// pointers-to-volatile.  The reason it's called readonly is not just because
// const is a keyword.  It's also because it's considered a property of the
// AnyPtr, not the thing being pointed to.
#pragma once
#include "../../uni/hash.h"
#include "type.h"
#include "access.h"

namespace ayu {

struct AnyPtr {
     // Usually putting metadata before data is faster but for some reason it
     // appears to work better this way.  TODO: reverify this
    Mu* address;
    union {
        const void* type_p;
        usize type_i;
    };

    constexpr AnyPtr (Null n = null) : address(n), type_p(null) { }
    AnyPtr (Type t, Mu* a) : address(a), type_p(t.data) { expect(t); }
    AnyPtr (Type t, Mu* a, bool readonly) :
        address(a), type_i(reinterpret_cast<usize>(t.data) | readonly)
    { expect(t); }
    AnyPtr (Type t, Mu* a, AccessCaps caps) :
        AnyPtr(t, a, !(caps % AC::Write))
    { require(caps & AC::Address); } // TODO: raise exception instead

     // Coercion from pointer is explicit for AnyPtr* and AnyRef* to avoid
     // mistakes.  Watch out for when you're working with template parameters!
    template <Describable T> explicit(IsAnyPtrOrAnyRef<T>)
    AnyPtr (T* a) :
        address((Mu*)a),
        type_p(Type::For<T>().data)
    { }

     // Coercion from const pointer.
    template <Describable T> explicit(IsAnyPtrOrAnyRef<T>)
    AnyPtr (const T* a) :
        address((Mu*)a),
        type_i(reinterpret_cast<usize>(Type::For<T>().data) | 1)
    { }

     // Returns false if this AnyPtr is either (typed) null or (typeless)
     // empty.
    explicit operator bool () const { return address; }
     // Returns true only for the typeless empty AnyPtr.
    bool empty () const { return !!type_p; }

    Type type () const {
        return Type((const void*)(type_i & ~1));
    }

    bool readonly () const { return type_i & 1; }
    AnyPtr add_readonly () const {
        AnyPtr r = *this;
        r.type_i |= 1;
        return r;
    }
    AnyPtr remove_readonly () const {
        AnyPtr r = *this;
        r.type_i &= ~1;
        return r;
    }

    AnyPtr try_upcast_to (Type to) const {
        Mu* p = dynamic_try_upcast(type(), to, address);
        return AnyPtr(to, p, readonly());
    }
    template <ConstableDescribable T>
    T* try_upcast_to () const {
        if (!std::is_const_v<T> && readonly()) return null;
        auto to = Type::For<std::remove_const_t<T>>();
        return (T*)dynamic_try_upcast(type(), to, address);
    }

    AnyPtr upcast_to (Type to) const {
        Mu* p = dynamic_upcast(type(), to, address);
        return AnyPtr(to, p, readonly());
    }
    template <ConstableDescribable T>
    T* upcast_to () const {
        if (!std::is_const_v<T> && readonly()) {
            raise(e_General, "Tried to cast readonly AnyPtr to non-const pointer (details NYI)");
        }
        auto to = Type::For<std::remove_const_t<T>>();
        return (T*)dynamic_upcast(type(), to, address);
    }

    template <ConstableDescribable T>
    T* expect_exact () const {
        expect(type() == Type::For<std::remove_const_t<T>>());
        expect(readonly() == std::is_const_v<T>);
        return reinterpret_cast<T*>(address);
    }

    template <ConstableDescribable T>
    operator T* () const { return upcast_to<T>(); }

     // Get the AccessCaps for this AnyPtr.  The only bit AnyPtr can represent
     // is the Writeable (here Readonly) bit.
    AccessCaps caps () const {
        return AC::AllowEverything ^ AccessCaps(readonly());
    }
};

 // AnyPtrs have a slightly evil property where a readonly pointer can equal a
 // non-readonly pointer.  This may be unintuitive, but it matches the behavior
 // of native C++ pointers and also makes looking them up in a hash table much
 // easier.
inline bool operator == (const AnyPtr& a, const AnyPtr& b) {
    return a.address == b.address && a.type() == b.type();
}
inline auto operator <=> (const AnyPtr& a, const AnyPtr& b) {
    return a.address == b.address
        ? a.type() <=> b.type()
        : a.address <=> b.address;
}

} // namespace ayu

template <>
struct std::hash<ayu::AnyPtr> {
    std::size_t operator () (const ayu::AnyPtr& p) const {
        return uni::hash_combine(
            std::hash<ayu::Mu*>{}(p.address),
            std::hash<ayu::Type>{}(p.type())
        );
    }
};
