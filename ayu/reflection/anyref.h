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
// AnyRefs can be read from with read() which takes a callback or get_as<>()
// which returns the referenced value after copying it with operator=.
//
// AnyRefs can be written with write() which takes a callback or set_as<>()
// which assigns the referenced value with operator=.  write() may or may not
// clear the item's value before calling the callback, so if you want to keep
// the item's original value, use modify().  Some AnyRefs are readonly, and
// trying to write to them will throw WriteReadonly.
//
// An AnyRef can be implicitly cast to a raw C++ pointer if the item it points
// to is addressable.  A readonly AnyRef can only be cast to a const pointer.  A
// raw C++ pointer can be implicitly cast to an AnyRef if the pointed-to type is
// known to AYU.
//
// There is an empty AnyRef, which has no type and no value.  There are also
// typed "null" AnyRefs, which have a type but no value, and are equivalent to
// typed null pointers.  operator bool returns false for both of these, so to
// differentiate them, call .type(), which will return the empty Type for the
// empty AnyRef.

#pragma once
#include <type_traits>
#include "access.internal.h"
#include "anyptr.h"

namespace ayu {

struct AnyRef {
    AnyPtr host;
    const in::Accessor* acr;

///// CONSTRUCTION

     // The empty AnyRef will cause null derefs if you do anything with it.
    constexpr AnyRef (Null n = null) : host(n), acr(n) { }

     // Construct from internal data.
    AnyRef (AnyPtr h, const in::Accessor* a) : host(h), acr(a) { }

     // Construct from a AnyPtr.
    AnyRef (AnyPtr p) : host(p), acr(null) { }

     // Construct from native pointer.  Explicit for AnyPtr* and AnyRef*,
     // because that's likely to be a mistake.
    template <ConstableDescribable T> explicit(IsAnyPtrOrAnyRef<T>)
    AnyRef (T* p) : host(p), acr(null) { }

     // Construct from unknown pointer and type
    AnyRef (Type t, Mu* p) : host(t, p), acr(null) { }
    AnyRef (Type t, Mu* p, bool readonly) : host(t, p, readonly), acr(null) { }

     // Construct from an object and an accessor for that object.  This is
     // intended to be used in computed_attrs and computed_elems functions in
     // AYU_DESCRIBE blocks.  The first argument must be an instance of the type
     // being described, and the second argument must be one of the
     // accessor-generating functions in describe-base.h (the same thing you
     // would pass to, say attr or elem).
    template <Describable From, AccessorFrom<From> Acr>
    AnyRef (From& h, Acr&& a) : AnyRef(&h, new Acr(move(a))) { }

     // Copy and move construction and assignment
    AnyRef (const AnyRef& o) : AnyRef(o.host, o.acr) {
        if (acr) [[unlikely]] acr->inc();
    }
    AnyRef (AnyRef&& o) :
        host(o.host), acr(o.acr)
    {
        o.host = null;
        o.acr = null;
    }
    AnyRef& operator = (const AnyRef& o) {
        this->~AnyRef();
        host = o.host;
        acr = o.acr;
        if (acr) [[unlikely]] acr->inc();
        return *this;
    }
    AnyRef& operator = (AnyRef&& o) {
        this->~AnyRef();
        host = o.host;
        acr = o.acr;
        o.host = null;
        o.acr = null;
        return *this;
    }

    constexpr ~AnyRef () { if (acr) [[unlikely]] acr->dec(); }

///// INFO

    explicit operator bool () const { return !!host; }

     // Get type of referred-to item
    Type type () const {
        Type r;
        access(AC::Read, AccessCB(r, [](Type& r, Type t, Mu*){
            r = t;
        }));
        return r;
    }

///// SIMPLE ACCESS

     // If false, address() will throw.
    bool addressable () const {
        return caps() % AC::Address;
    }

     // If false, attempting to write will throw.
    bool writeable () const {
        return caps() % AC::Write;
    }

     // Throws ReferenceUnaddressable if this AnyRef is not addressable.
    AnyPtr address () const {
        if (!acr) return host;
        AnyPtr r;
        access(AC::Address, AccessCB(r, [](AnyPtr& r, Type t, Mu* v){
            r = AnyPtr(t, v);
        }));
        if (!writeable()) r.add_readonly();
        return r;
    }

     // Can throw either TypeCantCast or ReferenceUnaddressable
    Mu* address_as (Type t) const {
        return address().upcast_to(t).address;
    }

    template <ConstableDescribable T>
    T* address_as () const {
        if constexpr (!std::is_const_v<T>) {
            if (!writeable()) raise_access_denied(AC::Write);
        }
        return (T*)address_as(Type::For<std::remove_const_t<T>>());
    }

     // Copying getter.
    template <ConstableDescribable T>
    T get_as () const {
         // TODO: detect value_func(s) and avoid a copy
        T r;
        read(AccessCB(r, [](T& r, Type t, Mu* v){
            r = *AnyPtr(t, v).upcast_to<T>();
        }));
        return r;
    }

     // Assign to the referenced item with lvalue ref.  This could be slightly
     // dangerous if you call this with a type that's the base of the existing
     // type, as it can "slice" the existing object, similarly to normal C++
     // assignment.
    template <Describable T>
    void set_as (const T& new_v) {
        write(AccessCB(new_v, [](const T& new_v, Type t, Mu* v){
            *AnyPtr(t, v).upcast_to<T>() = new_v;
        }));
    }

     // Assign to the referenced item with rvalue ref.  Same slicing danger as
     // above.
    template <Describable T>
    void set_as (T&& new_v) {
        write(AccessCB(move(new_v), [](T&& new_v, Type t, Mu* v){
            *AnyPtr(t, v).upcast_to<T>() = move(new_v);
        }));
    }

     // Cast to pointer
    operator AnyPtr () const {
        return address();
    }

    template <ConstableDescribable T>
    operator T* () const {
        return address_as<T>();
    }

///// ARBITRARY ACCESS
     // See access.h for how to use these.

    void check_access (AccessCaps mode) const {
        if (!contains(caps(), mode)) { raise_access_denied(mode); }
    }

    void access (AccessCaps mode, AccessCB cb) const {
        check_access(mode);
        if (acr) {
            acr->access(mode, *host.address, cb);
        }
        else cb(host.type(), host.address);
    }

    void read (AccessCB cb) const { access(AccessCaps::Read, cb); }
    void write (AccessCB cb) const { access(AccessCaps::Write, cb); }
    void modify (AccessCB cb) const { access(AccessCaps::Modify, cb); }

    AccessCaps caps () const {
        return acr ? host.caps() & acr->caps : host.caps();
    }

///// SYNTAX SUGAR FOR TRAVERSAL

     // These are just wrappers around item_attr and item_elem, but they're
     // extern so that we don't pull too many dependencies into this header.
    AnyRef operator [] (const AnyString& key) const;
    AnyRef operator [] (u32 index) const;

///// ERRORS

    [[noreturn]] void raise_access_denied (AccessCaps) const;
};

///// COMPARISON

 // AnyRef comparison is best-effort.  If two AnyRefs were constructed
 // differently but happen to point to the same item, they might be considered
 // unequal.  This should be rare though.  Similarly to AnyPtr (and native
 // pointers) access capabilities are ignored when comparing AnyRefs, so a
 // readonly or unaddressable ref may compare equal to a writeable or
 // addressable ref (provided other details of the refs are identical).
inline bool operator == (const AnyRef& a, const AnyRef& b) {
    if (a.host != b.host) return false;
    if (!a.acr | !b.acr) return a.acr == b.acr;
    return *a.acr == *b.acr;
}

///// ERROR CODES

 // Tried to write through a readonly AnyRef.
constexpr ErrorCode e_WriteReadonly = "ayu::e_WriteReadonly";
 // Tried to get the address of an AnyRef, but it doesn't support addressing.
constexpr ErrorCode e_AddressUnaddressable = "ayu::e_AddressUnaddressable";
 // Generic access denied (unknown reason; should probably never happen)
constexpr ErrorCode e_AccessDenied = "ayu::e_AccessDenied";

} // ayu

///// HASH

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

///// CHEATING

namespace ayu::in {

 // Disable destructor for a ref we know doesn't need it
union FakeRef {
    AnyRef ref;
    ~FakeRef () { expect(!ref.acr); }
};

} // ayu::in
