// A Reference is a reference-like class that can point to an item of any type
// that is known to AYU, that is, any type that has an AYU_DESCRIBE description.
//
// A Reference can reference any item that can be accessed through an accessor
// (see describe-base.h), even if its address cannot be taken.  So for instance,
// if a class has an abstract property that can only be accessed with methods
// called "get_size" and "set_size", then a Reference would let you refer to
// that abstract property as though it's a single item.
//
// Just as with C++ native references or pointers, there is no way to check that
// the lifetime of the Reference does not exceed the lifetime of the referred-to
// item, so take care not to dereference a Reference after its item goes away.
//
// Objects of the Reference class themselves are immutable.  Internally they
// contain a raw pointer to a parent object and a possibly-refcounted pointer to
// an accessor, so they are cheap to copy, but not threadsafe.
//
// References can be read from with read_as<> which takes a callback or get_as<>
// which returns the referenced value after copying it with operator=.
//
// References can be written with write_as<> which takes a callback or set_as<>
// which assigns the referenced value with operator=.  write_as<> may or may not
// clear the item's value before passing a reference to the callback, so if you
// want to keep the item's original value, use modify_as<>.  Some References are
// readonly, and trying to write to them will throw WriteReadonlyReference.
//
// A Reference can be implicitly cast to a raw C++ pointer if the item it points
// to is addressable (i.e. the internal accessor supports the address
// operation).  A readonly Reference can only be cast to a const pointer.  A raw
// C++ pointer can be implicitly cast to a Reference if the pointed-to type is
// known to AYU.
//
// There is an empty Reference, which has no type and no value.  There are also
// typed "null" References, which have a type but no value, and are equivalent
// to typed null pointers.  operator bool returns false for both of these, so to
// differentiate them, call .type(), which will return the empty Type for the
// empty Reference.  .address() will return null for null References and
// segfault for the empty Reference.
//
// References cannot be constructed until main() starts (except for the typeless
// empty Reference).

#pragma once

#include <type_traits>

#include "internal/accessors-internal.h"
#include "internal/descriptors-internal.h"
#include "dynamic.h"
#include "pointer.h"
#include "serialize-compound.h"

namespace ayu {

struct Reference {
    const Pointer host;
    const in::Accessor* const acr;

     // The empty value will cause null derefs if you do anything with it.
    constexpr Reference (Null n = null) : host(n), acr(n) { }
     // Construct from internal data.
    Reference (Pointer h, const in::Accessor* a) : host(h), acr(a) { }
     // Construct from a Pointer.
    constexpr Reference (Pointer p) : host(p), acr(null) { }
     // Construct from native pointer.  Watch out!  If you accidentally pass the
     // address of a Reference instead of a Reference itself, you will take a
     // Reference to the Reference instead of doing a copy construct!
    template <class T> requires (!std::is_same_v<T, Mu>)
    Reference (T* p) : host(p), acr(null) { }
     // Construct from unknown pointer and type
    Reference (Type t, Mu* p) : host(t, p), acr(null) { }
     // For use in attr_func and elem_func.
    template <class From, class Acr> requires (
        std::is_same_v<typename Acr::AccessorFromType, From>
    )
    Reference (From& h, Acr&& a) : Reference(&h, new Acr(move(a))) { }
     // Copy and move construction and assignment
    Reference (const Reference& o) : Reference(o.host, o.acr) {
        if (acr) acr->inc();
    }
    Reference (Reference&& o) :
        host(o.host), acr(o.acr)
    {
        const_cast<Pointer&>(o.host) = null;
        const_cast<const in::Accessor*&>(o.acr) = null;
    }
    Reference& operator = (const Reference& o) {
        this->~Reference();
        new (this) Reference(o);
        return *this;
    }
    Reference& operator = (Reference&& o) {
        this->~Reference();
        new (this) Reference(move(o));
        return *this;
    }

    ~Reference () { if (acr) acr->dec(); }

    explicit operator bool () const { return !!host; }
     // Get type of referred-to item
    Type type () const { return acr ? acr->type(host.address) : host.type; }

     // Writing through this reference throws if this is true
    bool readonly () const {
        bool r = host.type.readonly();
        if (acr) r |= !!(acr->accessor_flags & in::ACR_READONLY);
        return r;
    }

    [[noreturn]] void raise_WriteReadonly () const;
    void require_writeable () const {
        if (readonly()) raise_WriteReadonly();
    }

     // Returns null if this reference is not addressable.
    Mu* address () const {
        return acr ? acr->address(*host.address) : host.address;
    }
     // Can throw CannotCoerce, even if the result is null.
    Mu* address_as (Type t) const {
        return type().cast_to(t, address());
    }
    template <class T>
    T* address_as () const {
        if constexpr (!std::is_const_v<T>) {
            require_writeable();
        }
        return (T*)address_as(Type::CppType<T>());
    }

    [[noreturn]] void raise_Unaddressable () const;
    Mu* require_address () const {
        if (!*this) return null;
        if (auto a = address()) return a;
        else raise_Unaddressable();
    }
     // Can throw either CannotCoerce or UnaddressableReference
    Mu* require_address_as (Type t) const {
        return type().cast_to(t, require_address());
    }
    template <class T>
    T* require_address_as () const {
        return (T*)require_address_as(Type::CppType<T>());
    }

     // Read with callback
    void read (CallbackRef<void(Mu&)> cb) const {
        access(in::ACR_READ, cb);
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
    void write (CallbackRef<void(Mu&)> cb) const { access(in::ACR_WRITE, cb); }
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
    void modify (CallbackRef<void(Mu&)> cb) const { access(in::ACR_MODIFY, cb); }
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
    operator Pointer () const {
        return Pointer(type(), require_address());
    }

    template <class T>
    operator T* () const {
        return require_address_as<T>();
    }

     // These are used by serialize.  They will be most efficient if this
     // Reference has an address().
    Reference chain (const in::Accessor*) const noexcept;
    Reference chain_attr_func (Reference(*)(Mu&, AnyString), AnyString) const;
    Reference chain_elem_func (Reference(*)(Mu&, usize), usize) const;
     // Kinda internal, TODO move to internal namespace
    void access (in::AccessMode mode, CallbackRef<void(Mu&)> cb) const {
        if (mode != in::ACR_READ) require_writeable();
        if (acr) {
            acr->access(mode, *host.address, cb);
        }
        else {
            cb(*host.address);
        }
    }

     // Syntax sugar.
    Reference operator [] (AnyString key) const {
        return item_attr(*this, move(key));
    }
    Reference operator [] (usize index) const {
        return item_elem(*this, index);
    }
};

 // Reference comparison is best-effort.  References compare equal if:
 //  1. They have the same host and accessor pointers, or
 //  2. They have the same type, they both have an address(), and those
 //     addresses are equal.
 // This means that unaddressable references constructed through attr_func or
 // elem_func will not be comparable, and thus cannot be serialized.  Those
 // references are likely to be very inefficient anyway, so try not to create
 // them.
inline bool operator == (const Reference& a, const Reference& b) {
    if (a.host == b.host && a.acr == b.acr) return true;
    if (!a | !b) return false;
    if (a.type() != b.type()) return false;
    auto aa = a.address();
    return aa && aa == b.address();
}
inline bool operator != (const Reference& a, const Reference& b) {
    return !(a == b);
}

///// Reference error codes

 // Tried to write through a readonly reference.
constexpr ErrorCode e_ReferenceReadonly = "ayu::e_ReferenceReadonly";
 // Tried to get the address of a reference, but it doesn't support addressing.
constexpr ErrorCode e_ReferenceUnaddressable = "ayu::e_RefereneUnaddressable";

} // ayu

 // Allow Reference to be a key in unordered_map
template <>
struct std::hash<ayu::Reference> {
    size_t operator () (const ayu::Reference& r) const {
         // This is in a different order than operator==, but I don't think
         // that should be a problem, assuming the address is deterministic.
        auto a = r.address();
        if (a) return ayu::in::hash_combine(
            hash<void*>()((void*)a),
            hash<ayu::Type>()(r.type())
        );
        else return ayu::in::hash_combine(
            hash<ayu::Pointer>()(r.host),
            hash<void*>()((void*)r.acr)
        );
    }
};

