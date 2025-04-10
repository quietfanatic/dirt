// This module contains the classes implementing the accessors that can be used
// in AYU_DESCRIBE descriptions.

#pragma once

#include <typeinfo>

#include "../../uni/lilac.h"
#include "../common.h"
#include "../data/tree.h"
#include "anyptr.h"
#include "type.h"

namespace ayu::in {

///// UNIVERSAL ACCESSOR STUFF

enum class AcrFlags : u8 {
     // Make TreeFlags-equivalent values the same value for optimization.
    PreferHex = 0x1,
    PreferCompact = 0x2,
    PreferExpanded = 0x4,
    AllTreeFlags = 0x7,
     // Writes through this accessor will fail.  Attrs and elems with this
     // accessor will not be serialized.
    Readonly = 0x20,
     // Children considered addressable even if this item is not addressable.
    PassThroughAddressable = 0x40,
     // Consider this item unaddressable even if it normally would be
    Unaddressable = 0x80,
};
DECLARE_ENUM_BITWISE_OPERATORS(AcrFlags)

 // These belong on AttrDcr and ElemDcr, but we're putting them with the
 // accessor flags to save space.
enum class AttrFlags : u8 {
     // If this is set, the attr doesn't need to be present when doing
     // the from_tree operation.  There's no support for default values here;
     // if an attr wants a default value, set it in the class's default
     // constructor.  This is allowed on elems, but all optional elems must
     // follow all non-optional elems (allowing optional elems in the middle
     // would change the apparent index of later required elems, which would
     // be confusing).
    Optional = 0x1,
     // If this is set, the attrs of this attr will be included in the
     // serialization of this item and available through calls to attr().  In
     // addition, this item will be able to be upcasted to the type of the attr
     // if it is addressable.  This is not currently supported on elems.
    Include = 0x2,
     // If this is set, the attr will not be serialized in to_tree.
    Invisible = 0x4,
     // If this is set, the attr will not be deserialized in from_tree.
    Ignored = 0x8,
     // If this is set, there is a tree 16 bytes before the attr's key, which is
     // the default value of the attr.
    HasDefault = 0x10,
     // If this is set, map an empty array to the attribute being missing from
     // the object, and an array of one element to the attribute being present
     // with that element as its value.
    CollapseOptional = 0x20,
};
DECLARE_ENUM_BITWISE_OPERATORS(AttrFlags)

 // Instead of having separate methods for each type of access, we're using the
 // same method for all of them, and using an enum to differentiate.  This saves
 // a lot of code size, because a lot of ACRs have nearly or exactly the same
 // methods for all access operations.  Even manually demerging identical access
 // methods and storing the same pointer three times in the VT compiles larger
 // than this.
 //
 // Don't worry about these values :3
enum class AccessMode {
     // Requests an AnyPtr to either the original item or a copy that will go
     // out of scope after the callback.  The AnyPtr will only be readonly if
     // the item's type is const.  You should not write to this; writes to the
     // AnyPtr may or may not be written to the item
    Read = 0x1,
     // Requests an AnyPtr to either the original item or a default-constructed
     // value which will be written back to the item after the callback.
     // Neglecting to write to the AnyPtr in the callback may clear the object.
    Write = 0x2,
     // Requests an AnyPtr to either the original item or a copy which will be
     // written back after the callback.  May be implemented by a
     // read-modify-write sequence.
    Modify = 0x0
};

 // Use the weird values we selected to optimize this common operation
inline
AccessMode write_to_modify (AccessMode mode) {
    return AccessMode(int(mode) & ~int(AccessMode::Write));
}


struct Accessor;
 // This is the callback passed to access operations.
using AccessCB = CallbackRef<void(AnyPtr, bool)>;
 // This is the "virtual function" that accessors use
using AccessFunc = void(const Accessor*, AccessMode, Mu&, AccessCB);

void delete_Accessor (Accessor*) noexcept;

 // Not quite full type information, but enough to destroy and compare for
 // equality.
enum class AccessorStructure : u8 {
    Flat,
    Variable,
    Chain,
    ChainAttrFunc,
    ChainElemFunc,
    ChainDataFunc,
};
using AS = AccessorStructure;

 // The base class for all accessors.  Try to keep this small.
struct Accessor {
    AccessFunc* access_func;
     // If ref_count is 0, this is a constexpr accessor and it can't be
     // modified.  Yes, this does mean that if an accessor accumulates enough
     // references to overflow the count it won't be deleted.  I doubt you'll
     // care.  (Usually refcounts are marked mutable, but that's illegal in
     // constexpr classes.)  Note also that the refcount starts at 1, so when
     // constructing an AnyRef or a ChainAcr with a new Accessor*, don't call
     // inc() on it.
    u32 ref_count = 1;
    AccessorStructure structure;
    AcrFlags flags = {};
     // These belong on AttrDcr and ElemDcr but we're storing them here to
     //  save space.
    AttrFlags attr_flags = {};

    explicit constexpr Accessor (
        AccessFunc* a, AccessorStructure s, AcrFlags f = {}
    ) :
        access_func(a), structure(s), flags(f)
    { }

    TreeFlags tree_flags () const {
        return TreeFlags(flags & AcrFlags::AllTreeFlags);
    }

    void access (AccessMode mode, Mu& from, AccessCB cb) const {
        expect(mode == AccessMode::Read ||
               mode == AccessMode::Write ||
               mode == AccessMode::Modify
        );
        if (mode != AccessMode::Read) {
            expect(!(flags & AcrFlags::Readonly));
        }
        access_func(this, mode, from, cb);
    }
    void read (Mu& from, AccessCB cb) const {
        access(AccessMode::Read, from, cb);
    }
    void write (Mu& from, AccessCB cb) const {
        access(AccessMode::Write, from, cb);
    }
    void modify (Mu& from, AccessCB cb) const {
        access(AccessMode::Modify, from, cb);
    }
    AnyPtr address (Mu& from) const {
        AnyPtr r;
        access(AccessMode::Read, from,
            AccessCB(r, [](AnyPtr& r, AnyPtr v, bool addr){
                if (!addr) v.address = null;
                r = v;
            })
        );
        return r;
    }

    void inc () const {
         // Unlikely because most ACRs are constexpr.  This cannot be converted
         // to branchless code because the acr may be in a readonly region.
        if (ref_count) [[unlikely]] {
            const_cast<u32&>(ref_count)++;
        }
    }
    NOINLINE // noinline this so it doesn't make the caller allocate stack space
    void do_dec () {
        if (!--ref_count) {
            delete_Accessor(this);
        }
    }
    void dec () const {
        if (ref_count) [[unlikely]] const_cast<Accessor*>(this)->do_dec();
    }
    static void* operator new (usize s) {
        return lilac::allocate_fixed_size(s);
    }
     // We might be deleting from a base class when we don't know the derived
     // class, so use unsized delete.
    static void operator delete (void* p) {
        lilac::deallocate_unknown_size(p);
    }
};

 // Yes Accessors are comparable!  Two Accessors are the same if they come from
 // the same place in the same AYU_DESCRIBE block, or if they're dynamically
 // generated from the same inputs.
bool operator== (const Accessor&, const Accessor&);
usize hash_acr (const Accessor&);

template <class Acr>
constexpr Acr constexpr_acr (Acr a) {
    a.ref_count = 0;
    return a;
}

///// ACCESSOR TYPES

/// member

struct MemberAcr0 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct MemberAcr2 : MemberAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    const Description* const* desc;
    To From::* mp;
    explicit constexpr MemberAcr2 (To From::* mp, AcrFlags flags = {}) :
        MemberAcr0(&_access, AS::Flat, flags),
        desc(get_indirect_description<To>()),
        mp(mp)
    { }
};

/// base

template <class From, class To>
struct BaseAcr2 : Accessor {
    using AcrFromType = From;
    using AcrToType = To;
    static void _access (
        const Accessor* acr, AccessMode, Mu& from, AccessCB cb
    ) {
         // reinterpret then implicit upcast
        To& to = reinterpret_cast<From&>(from);
        cb(AnyPtr(&to), !(acr->flags & AcrFlags::Unaddressable));
    }
    explicit constexpr BaseAcr2 (AcrFlags flags = {}) :
        Accessor(&_access, AS::Flat, flags)
    { }
};

 // Optimization for when base is at the same address as derived
struct FirstBaseAcr0 : Accessor {
    const Description* const* desc;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    explicit constexpr FirstBaseAcr0 (
        const Description* const* d, AcrFlags flags = {}
    ) :
        Accessor(&_access, AS::Flat, flags),
        desc(d)
    { }
};
template <class From, class To>
struct FirstBaseAcr2 : FirstBaseAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    explicit constexpr FirstBaseAcr2 (AcrFlags flags = {}) :
        FirstBaseAcr0(get_indirect_description<To>(), flags)
    { }
};

/// ref_func

struct RefFuncAcr0 : Accessor {
     // It's the programmer's responsibility to know whether they're
     // allowed to address this reference or not.
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct RefFuncAcr2 : RefFuncAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    const Description* const* desc;
    To&(* f )(From&);
    explicit constexpr RefFuncAcr2 (To&(* f )(From&), AcrFlags flags = {}) :
        RefFuncAcr0(&_access, AS::Flat, flags),
        desc(get_indirect_description<To>()),
        f(f)
    { }
};

/// const_ref_func

struct ConstRefFuncAcr0 : Accessor {
     // It's the programmer's responsibility to know whether they're
     // allowed to address this reference or not.
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct ConstRefFuncAcr2 : ConstRefFuncAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    const Description* const* desc;
    const To&(* f )(const From&);
    explicit constexpr ConstRefFuncAcr2 (
        const To&(* f )(const From&), AcrFlags flags = {}
    ) :
        ConstRefFuncAcr0(&_access, AS::Flat, flags),
        desc(get_indirect_description<To>()),
        f(f)
    { }
};

/// const_ref_funcs

template <class To>
struct RefFuncsAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct RefFuncsAcr2 : RefFuncsAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    const To&(* getter )(const From&);
    void(* setter )(From&, const To&);
    explicit constexpr RefFuncsAcr2 (
        const To&(* g )(const From&),
        void(* s )(From&, const To&),
        AcrFlags flags = {}
    ) :
        RefFuncsAcr1<To>(&RefFuncsAcr1<To>::_access, AS::Flat, flags),
        getter(g), setter(s)
    { }
};
template <class To>
void RefFuncsAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu& from, AccessCB cb
) {
    auto self = static_cast<const RefFuncsAcr2<Mu, To>*>(acr);
    switch (mode) {
        case AccessMode::Read: {
            To tmp = self->getter(from);
            cb(AnyPtr(&tmp), false);
        } return;
        case AccessMode::Write: {
            To tmp;
            cb(AnyPtr(&tmp), false);
            self->setter(from, tmp);
        } return;
        case AccessMode::Modify: {
            To tmp = self->getter(from);
            cb(AnyPtr(&tmp), false);
            self->setter(from, tmp);
        } return;
        default: never();
    }
}

/// value_func

template <class To>
struct ValueFuncAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct ValueFuncAcr2 : ValueFuncAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    To(* f )(const From&);
    explicit constexpr ValueFuncAcr2 (To(* f )(const From&), AcrFlags flags = {}) :
        ValueFuncAcr1<To>(
            &ValueFuncAcr1<To>::_access, AS::Flat,
            flags | AcrFlags::Readonly
        ),
        f(f)
    { }
};
template <class To>
void ValueFuncAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu& from, AccessCB cb
) {
    expect(mode == AccessMode::Read);
    auto self = static_cast<const ValueFuncAcr2<Mu, To>*>(acr);
    const To tmp = self->f(from);
    cb(AnyPtr(&tmp), false);
}

/// value_funcs

template <class To>
struct ValueFuncsAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct ValueFuncsAcr2 : ValueFuncsAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    To(* getter )(const From&);
    void(* setter )(From&, To);
    explicit constexpr ValueFuncsAcr2 (
        To(* g )(const From&),
        void(* s )(From&, To),
        AcrFlags flags = {}
    ) :
        ValueFuncsAcr1<To>(&ValueFuncsAcr1<To>::_access, AS::Flat, flags),
        getter(g), setter(s)
    { }
};
template <class To>
void ValueFuncsAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu& from, AccessCB cb
) {
    auto self = static_cast<const ValueFuncsAcr2<Mu, To>*>(acr);
    switch (mode) {
        case AccessMode::Read: {
            To tmp = self->getter(from);
            cb(AnyPtr(&tmp), false);
        } return;
        case AccessMode::Write: {
            To tmp;
            cb(AnyPtr(&tmp), false);
            self->setter(from, tmp);
        } return;
        case AccessMode::Modify: {
            To tmp = self->getter(from);
            cb(AnyPtr(&tmp), false);
            self->setter(from, move(tmp));
        } return;
        default: never();
    }
     // Feels like this should compile smaller but it doesn't, probably because
     // mode has to be saved through the function calls.
    //To tmp = mode != AccessMode::Write ? self->getter(from) : To();
    //cb(AnyPtr(&tmp), false);
    //if (mode != AccessMode::Read) self->setter(from, move(tmp));
}

/// mixed_funcs

template <class To>
struct MixedFuncsAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct MixedFuncsAcr2 : MixedFuncsAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    To(* getter )(const From&);
    void(* setter )(From&, const To&);
    explicit constexpr MixedFuncsAcr2 (
        To(* g )(const From&),
        void(* s )(From&, const To&),
        AcrFlags flags = {}
    ) :
        MixedFuncsAcr1<To>(&MixedFuncsAcr1<To>::_access, AS::Flat, flags),
        getter(g), setter(s)
    { }
};
template <class To>
void MixedFuncsAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu& from, AccessCB cb
) {
    auto self = static_cast<const MixedFuncsAcr2<Mu, To>*>(acr);
    switch (mode) {
        case AccessMode::Read: {
            To tmp = self->getter(from);
            cb(AnyPtr(&tmp), false);
        } return;
        case AccessMode::Write: {
            To tmp;
            cb(AnyPtr(&tmp), false);
            self->setter(from, tmp);
        } return;
        case AccessMode::Modify: {
            To tmp = self->getter(from);
            cb(AnyPtr(&tmp), false);
            self->setter(from, tmp);
        } return;
        default: never();
    }
}

/// assignable

template <class From, class To>
struct AssignableAcr2 : Accessor {
    using AcrFromType = From;
    using AcrToType = To;
    static void _access (
        const Accessor*, AccessMode mode, Mu& from_mu, AccessCB cb
    ) {
        From& from = reinterpret_cast<From&>(from_mu);
        To tmp;
        if (mode != AccessMode::Write) tmp = from;
        cb(AnyPtr(&tmp), false);
        if (mode != AccessMode::Read) from = tmp;
        return;
    }
    explicit constexpr AssignableAcr2 (AcrFlags flags = {}) :
        Accessor(&_access, AS::Flat, flags)
    { }
};

/// variable

template <class To>
struct VariableAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct VariableAcr2 : VariableAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    const Description* const* desc;
    alignas(usize) mutable To value;
     // This ACR cannot be constexpr.
    explicit VariableAcr2 (To&& v, AcrFlags flags = {}) :
        VariableAcr1<To>(
            &VariableAcr1<To>::_access, AS::Variable,
            get_indirect_description<To>(), flags
        ),
        desc(get_indirect_description<To>()),
        value(move(v))
    { }
};
template <class To>
void VariableAcr1<To>::_access (
    const Accessor* acr, AccessMode, Mu&, AccessCB cb
) {
    auto self = static_cast<const VariableAcr2<Mu, To>*>(acr);
     // This ACR cannot be addressable, because then chaining may take the
     // address but then release this ACR object, invalidating the reference.
    cb(AnyPtr(&self->value), false);
}

/// constant

template <class To>
struct ConstantAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From, class To>
struct ConstantAcr2 : ConstantAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    const Description* const* desc;
    alignas(usize) const To value;  // The offset of this MUST match VariableAcr2::value
    explicit constexpr ConstantAcr2 (const To& v, AcrFlags flags = {}) :
        ConstantAcr1<To>(
            &ConstantAcr1<To>::_access, AS::Variable,
            flags | AcrFlags::Readonly
        ),
        desc(get_indirect_description<To>()),
        value(v)
    { }
};
template <class To>
void ConstantAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu&, AccessCB cb
) {
    expect(mode == AccessMode::Read);
    auto self = static_cast<const ConstantAcr2<Mu, To>*>(acr);
    cb(&self->value, false);
}

/// constant_pointer

struct ConstantPtrAcr0 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};

template <class From, class To>
struct ConstantPtrAcr2 : ConstantPtrAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    const Description* const* desc;
    const To* pointer;
    explicit constexpr ConstantPtrAcr2 (const To* p, AcrFlags flags = {}) :
        ConstantPtrAcr0(
            &_access, AS::Flat,
            get_indirect_description<To>(),
            flags | AcrFlags::Readonly
        ),
        desc(get_indirect_description<To>()),
        pointer(p)
    { }
};

/// anyref_func

 // This is a little awkward because we can't transfer the flags from the
 // calculated AnyRef's acr to this one.  We'll just have to hope we don't
 // miss anything important.
struct AnyRefFuncAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From>
struct AnyRefFuncAcr2 : AnyRefFuncAcr1 {
    using AcrFromType = From;
    using AcrToType = AnyRef;
    AnyRef(* f )(From&);
    explicit constexpr AnyRefFuncAcr2 (
        AnyRef(* f )(From&), AcrFlags flags = {}
    ) :
        AnyRefFuncAcr1(&_access, AS::Flat, flags), f(f)
    { }
};

/// anyptr_func

struct AnyPtrFuncAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};
template <class From>
struct AnyPtrFuncAcr2 : AnyPtrFuncAcr1 {
    using AcrFromType = From;
    using AcrToType = AnyPtr;
    AnyPtr(* f )(From&);
    explicit constexpr AnyPtrFuncAcr2 (
        AnyPtr(* f )(From&), AcrFlags flags = {}
    ) :
        AnyPtrFuncAcr1(&_access, AS::Flat, flags), f(f)
    { }
};

} // namespace ayu::in

template <>
struct std::hash<ayu::in::Accessor> {
    size_t operator () (const ayu::in::Accessor& acr) const {
        return ayu::in::hash_acr(acr);
    }
};
