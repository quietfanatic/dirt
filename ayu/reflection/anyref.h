// An AnyRef is a reference-like class that can point to an item of any type
// that is known to AYU, that is, any type that has an AYU_DESCRIBE description.
//
// An AnyRef can reference any item that can be accessed through an accessor
// (see describe-base.h), even if its address cannot be taken.  So for instance,
// if a class has an abstract property that can only be accessed with methods
// called "get_size" and "set_size", then a AnyRef would let you refer to
// that abstract property as though it's a single item.
//
// Just as with C++ native references or pointers, there is no way to check that
// the lifetime of the AnyRef does not exceed the lifetime of the referred-to
// item, so take care not to dereference a AnyRef after its item goes away.
//
// Objects of the AnyRef class themselves are immutable.  Internally they
// contain a raw pointer to a parent object and a possibly-refcounted pointer to
// an accessor, so they are cheap to copy, but not threadsafe.
//
// AnyRefs can be read from with read_as<> which takes a callback or get_as<>
// which returns the referenced value after copying it with operator=.
//
// AnyRefs can be written with write_as<> which takes a callback or set_as<>
// which assigns the referenced value with operator=.  write_as<> may or may not
// clear the item's value before passing a reference to the callback, so if you
// want to keep the item's original value, use modify_as<>.  Some AnyRefs are
// readonly, and trying to write to them will throw WriteReadonly.
//
// A AnyRef can be implicitly cast to a raw C++ pointer if the item it points
// to is addressable (i.e. the internal accessor supports the address
// operation).  A readonly AnyRef can only be cast to a const pointer.  A raw
// C++ pointer can be implicitly cast to a AnyRef if the pointed-to type is
// known to AYU.
//
// There is an empty AnyRef, which has no type and no value.  There are also
// typed "null" AnyRefs, which have a type but no value, and are equivalent to
// typed null pointers.  operator bool returns false for both of these, so to
// differentiate them, call .type(), which will return the empty Type for the
// empty AnyRef.  .address() will return null for null AnyRefs and segfault for
// the empty AnyRef.
//
// AnyRefs cannot be constructed until main() starts (except for the typeless
// empty AnyRef).

#pragma once

#include <type_traits>

#include "accessors.internal.h"
#include "anyptr.h"

namespace ayu {

struct AnyRef {
    AnyPtr host;
    const in::Accessor* acr;

     // The empty AnyRef will cause null derefs if you do anything with it.
    constexpr AnyRef (Null n = null) : host(n), acr(n) { }
     // Construct from internal data.
    constexpr AnyRef (AnyPtr h, const in::Accessor* a) : host(h), acr(a) { }
     // Construct from a AnyPtr.
    constexpr AnyRef (AnyPtr p) : host(p), acr(null) { }
     // Construct from native pointer.  Explicit for AnyPtr* and AnyRef*,
     // because that's likely to be a mistake.
    template <class T> requires (
        !std::is_same_v<std::remove_cv_t<T>, void> &&
        !std::is_same_v<std::remove_cv_t<T>, Mu>
    ) explicit (
        std::is_same_v<std::remove_cv_t<T>, AnyPtr> ||
        std::is_same_v<std::remove_cv_t<T>, AnyRef>
    ) AnyRef (T* p) : host(p), acr(null) { }
     // Construct from unknown pointer and type
    constexpr AnyRef (Type t, Mu* p) : host(t, p), acr(null) { }
     // For use in attr_func and elem_func.
    template <class From, class Acr> requires (
        std::is_same_v<typename Acr::AcrFromType, From>
    )
    AnyRef (From& h, Acr&& a) : AnyRef(&h, new Acr(move(a))) { }
     // Copy and move construction and assignment
    constexpr AnyRef (const AnyRef& o) : AnyRef(o.host, o.acr) {
        if (acr) acr->inc();
    }
    constexpr AnyRef (AnyRef&& o) :
        host(o.host), acr(o.acr)
    {
        o.host = null;
        o.acr = null;
    }
    constexpr AnyRef& operator = (const AnyRef& o) {
        this->~AnyRef();
        host = o.host;
        acr = o.acr;
        if (acr) acr->inc();
        return *this;
    }
    constexpr AnyRef& operator = (AnyRef&& o) {
        this->~AnyRef();
        host = o.host;
        acr = o.acr;
        o.host = null;
        o.acr = null;
        return *this;
    }

    constexpr ~AnyRef () { if (acr) acr->dec(); }

    explicit constexpr operator bool () const { return !!host; }
     // Get type of referred-to item
    constexpr Type type () const { return acr ? acr->type(host.address) : host.type; }

     // Writing through this reference throws if this is true
    constexpr bool readonly () const {
        bool r = host.type.readonly();
        if (acr) r |= !!(acr->flags & in::AcrFlags::Readonly);
        return r;
    }

    [[noreturn]] void raise_WriteReadonly () const;
    void require_writeable () const {
        if (readonly()) raise_WriteReadonly();
    }

    constexpr bool addressable () const {
        return !acr || !(acr->flags & in::AcrFlags::Unaddressable);
    }

     // Returns null if this reference is not addressable.
    constexpr Mu* address () const {
        return acr ? acr->address(*host.address) : host.address;
    }
     // Can throw CannotCoerce, even if the result is null.
    constexpr Mu* address_as (Type t) const {
        if (std::is_constant_evaluated()) {
            expect(!acr);
            require(t == host.type || t.remove_readonly() == host.type);
            return host.address;
        }
        else return type().cast_to(t, address());
    }
    template <class T>
    T* address_as () const {
        if constexpr (!std::is_const_v<T>) {
            require_writeable();
        }
        return (T*)address_as(Type::CppType<T>());
    }

    [[noreturn]] void raise_Unaddressable () const;
    constexpr Mu* require_address () const {
        if (!*this) return null;
        if (auto a = address()) return a;
        else raise_Unaddressable();
    }
     // Can throw either CannotCoerce or UnaddressableAnyRef
    constexpr Mu* require_address_as (Type t) const {
        if (std::is_constant_evaluated()) {
            expect(!acr);
            require(t == host.type || t.remove_readonly() == host.type);
            return host.address;
        }
        else return type().cast_to(t, require_address());
    }
    template <class T>
    T* require_address_as () const {
        return (T*)require_address_as(Type::CppType<T>());
    }

     // Read with callback
    void read (CallbackRef<void(Mu&)> cb) const {
        access(in::AccessMode::Read, cb);
    }
     // Cast and read with callback
    void read_as (Type t, CallbackRef<void(Mu&)> cb) const {
        read([this, t, cb](Mu& v){
            Mu& tv = *type().cast_to(t, &v);
            cb(tv);
        });
    }
    template <class T>
    void read_as (CallbackRef<void(T&)> cb) const {
        read_as(Type::CppType<T>(), cb.template reinterpret<void(Mu&)>());
    }
     // Write with callback
    void write (CallbackRef<void(Mu&)> cb) const {
        access(in::AccessMode::Write, cb);
    }
     // Cast and write with callback
    void write_as (Type t, CallbackRef<void(Mu&)> cb) const {
        write([this, t, cb](Mu& v){
            Mu& tv = *type().cast_to(t, &v);
            cb(tv);
        });
    }
    template <class T>
        requires (!std::is_const_v<T>)
    void write_as (CallbackRef<void(T&)> cb) const {
        write_as(Type::CppType<T>(), cb.template reinterpret<void(Mu&)>());
    }
     // Modify in-place with callback
    void modify (CallbackRef<void(Mu&)> cb) const {
        access(in::AccessMode::Modify, cb);
    }
     // Cast and modify in-place with callback
    void modify_as (Type t, CallbackRef<void(Mu&)> cb) const {
        write([this, t, cb](Mu& v){
            Mu& tv = *type().cast_to(t, &v);
            cb(tv);
        });
    }
    template <class T>
        requires (!std::is_const_v<T>)
    void modify_as (CallbackRef<void(T&)> cb) const {
        modify_as(Type::CppType<T>(), cb.template reinterpret<void(Mu&)>());
    }

     // Copying getter.  Preferentially uses address if it's available.
    template <class T>
    T get_as () const {
        if (Mu* a = address()) {
            return *reinterpret_cast<T*>(
                type().cast_to(Type::CppType<T>(), a)
            );
        }
        else {
            T r;
            read_as<T>([&r](const T& v){
                r = v;
            });
            return r;
        }
    }
     // Assign to the referenced item with rvalue ref.  Preferentially uses
     // address if it's available.
    template <class T>
        requires (!std::is_const_v<T>)
    void set_as (T&& new_v) {
        if (Mu* a = address()) {
            require_writeable();
            *reinterpret_cast<T*>(
                type().cast_to(Type::CppType<T>(), a)
            ) = move(new_v);
        }
        else {
            write_as<T>([&new_v](T& v){
                v = move(new_v);
            });
        }
    }
     // Assign to the referenced item with lvalue ref.
    template <class T>
        requires (!std::is_const_v<T>)
    void set_as (const T& new_v) {
        if (Mu* a = address()) {
            require_writeable();
            *reinterpret_cast<T*>(
                type().cast_to(Type::CppType<T>(), a)
            ) = new_v;
        }
        else {
            write_as<T>([&new_v](T& v){
                v = new_v;
            });
        }
    }

     // Cast to pointer
    operator AnyPtr () const {
        return AnyPtr(type(), require_address());
    }

    template <class T>
    operator T* () const {
        return require_address_as<T>();
    }

     // Kinda internal, TODO move to internal namespace
    void access (in::AccessMode mode, CallbackRef<void(Mu&)> cb) const {
        if (mode != in::AccessMode::Read) require_writeable();
        if (acr) {
            acr->access(mode, *host.address, cb);
        }
        else {
            cb(*host.address);
        }
    }

     // Syntax sugar.  These are just wrappers around item_attr and item_elem,
     // but they're extern so that we don't pull to many dependencies into this
     // header.
    AnyRef operator [] (const AnyString& key);
    AnyRef operator [] (usize index);
};

 // AnyRef comparison is best-effort.  If two AnyRefs were constructed
 // differently but happen to point to the same item, they might be considered
 // unequal.  This should be rare though.
constexpr bool operator == (const AnyRef& a, const AnyRef& b) {
    if (a.host != b.host) return false;
    if (!a.acr | !b.acr) return a.acr == b.acr;
    return *a.acr == *b.acr;
}
constexpr bool operator != (const AnyRef& a, const AnyRef& b) {
    return !(a == b);
}

///// AnyRef error codes

 // Tried to write through a readonly AnyRef.
constexpr ErrorCode e_WriteReadonly = "ayu::e_WriteReadonly";
 // Tried to get the address of an AnyRef, but it doesn't support addressing.
constexpr ErrorCode e_ReferenceUnaddressable = "ayu::e_ReferenceUnaddressable";

} // ayu

 // Allow AnyRef to be a key in unordered_map
template <>
struct std::hash<ayu::AnyRef> {
    std::size_t operator () (const ayu::AnyRef& r) const {
        return uni::hash_combine(
            hash<ayu::AnyPtr>()(r.host),
            r.acr ? hash<ayu::in::Accessor>()(*r.acr) : 0
        );
    }
};

