// This module contains the classes implementing the accessors that can be used
// in AYU_DESCRIBE descriptions.

#pragma once
#include "access.h"
#include <typeinfo>
#include "../../uni/lilac.h"
#include "../common.h"
#include "../data/tree.h"
#include "anyptr.h"
#include "type.h"

namespace ayu::in {

///// UNIVERSAL ACCESSOR STUFF

enum class AcrFlags {
     // Writes through this accessor will fail.  Attrs and elems with this
     // accessor will not be serialized.
    Readonly = u8(AC::Write), // Inverted!
     // Consider this item unaddressable even if it normally would be.
    Unaddressable = u8(AC::Address), // Inverted!
     // Children considered addressable even if this item is not addressable.
    ChildrenAddressable = u8(AC::AddressChildren), // Not inverted!

     // These are only used in the describe API.  They're transferred to actual
     // TreeFlags when the ACR is written.
    PreferHex = u8(TreeFlags::PreferHex) << 8,
    PreferCompact = u8(TreeFlags::PreferCompact) << 8,
    PreferExpanded = u8(TreeFlags::PreferExpanded) << 8,
};
DECLARE_ENUM_BITWISE_OPERATORS(AcrFlags)

constexpr AccessCaps acr_flags_to_access_caps (AcrFlags f) {
    return AC::Read | AccessCaps(u8(
         // Flip Readonly and Unaddressable
        (~f & (AcrFlags::Readonly | AcrFlags::Unaddressable)) |
         // Merge ~Unaddressable into ChildrenAddressable
        ((f | (~f) << 4) & AcrFlags::ChildrenAddressable)
    ));
}

constexpr TreeFlags acr_flags_to_tree_flags (AcrFlags f) {
    return TreeFlags(u8(f) >> 8);
}

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
     // serialization of this item and available through calls to attr().  This
     // is not currently supported on elems.
    Collapse = 0x2,
     // If this is set, this item will be able to be upcasted to the type of the
     // attr/elem if it is addressable.
    Castable = 0x4,
     // If this is set, the attr will not be serialized in to_tree.
    Invisible = 0x8,
     // If this is set, the attr will not be deserialized in from_tree.
    Ignored = 0x10,
     // If this is set, there is a tree 16 bytes before the attr's key, which is
     // the default value of the attr.
    HasDefault = 0x20,
     // If this is set, map an empty array to the attribute being missing from
     // the object, and an array of one element to the attribute being present
     // with that element as its value.
    CollapseOptional = 0x40,
     // For space optimization
    KeyLocal = 0x80
};
DECLARE_ENUM_BITWISE_OPERATORS(AttrFlags)

struct Accessor;
 // This is the "virtual function" that accessors use
using AccessFunc = void(const Accessor*, Mu&, AccessCB, AccessCaps);

void delete_Accessor (Accessor*) noexcept;

 // Arrange these in rough order of commonality for cachiness
enum class AcrForm : u8 {
    Functive, // Miscellaneous functive accessor that doesn't need destructing
    Noop,
    Member,
    RefFunc,
    ConstantPtr,
    AnyRefFunc,
    AnyPtrFunc,
    Variable,
    Chain,
    ChainAttrFunc,
    ChainElemFunc,
    ChainDataFunc,
};
using AF = AcrForm;

 // Access function lookup table.  GCC's switch-statement jump tables have
 // issues with register allocation, so we're using our own.
void access_Functive (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Typed (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Member (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_RefFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_ConstantPtr (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_AnyRefFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_AnyPtrFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Variable (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Chain (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_ChainAttrFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_ChainElemFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_ChainDataFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
constexpr AccessFunc* access_table [] = {
    access_Functive,
    access_Typed,
    access_Member,
    access_RefFunc,
    access_ConstantPtr,
    access_AnyRefFunc,
    access_AnyPtrFunc,
    access_Variable,
    access_Chain,
    access_ChainAttrFunc,
    access_ChainElemFunc,
    access_ChainDataFunc,
};

 // The base class for all accessors.  Try to keep this small.
struct Accessor {
     // If ref_count is 0, this is a constexpr accessor and it can't be
     // modified.  Yes, this does mean that if an accessor accumulates enough
     // references to overflow the count it won't be deleted.  I doubt you'll
     // care.  (Usually refcounts are marked mutable, but that's illegal in
     // constexpr classes.)  Note also that the refcount starts at 1, so when
     // constructing an AnyRef or a ChainAcr with a new Accessor*, don't call
     // inc() on it.
    u32 ref_count = 1;
    AcrForm form;
    AccessCaps caps;
    TreeFlags tree_flags;
     // These belong on AttrDcr and ElemDcr but we're storing them here to
     // save space.
    AttrFlags attr_flags = {};

     // Constructor for ad-hoc accessors.  The tree_flags and attr_flags should
     // never be used on this.
    explicit constexpr Accessor (AcrForm f, AccessCaps c) :
        form(f), caps(c), tree_flags()
    { }

    explicit constexpr Accessor (AcrForm s, AcrFlags f) :
        form(s),
        caps(acr_flags_to_access_caps(f)),
        tree_flags(acr_flags_to_tree_flags(f))
    { }

    void access (AccessCaps mode, Mu& from, AccessCB cb) const {
        expect(mode <= caps);
        access_table[u8(form)](this, from, cb, mode);
    }

    void read (Mu& from, AccessCB cb) const {
        access(AC::Read, from, cb);
    }
    void write (Mu& from, AccessCB cb) const {
        access(AC::Write, from, cb);
    }
    void modify (Mu& from, AccessCB cb) const {
        access(AC::Modify, from, cb);
    }

     // This doesn't really feel like it belongs here but it's too convenient
    AnyPtr address (Mu& from) const {
        if (!(caps % AC::Address)) return {};
        AnyPtr r;
        access(AC::Address, from,
            AccessCB(r, [](AnyPtr& r, Type t, Mu* v){
                r = AnyPtr(t, v);
            })
        );
        if (!(caps % AC::Write)) r = r.add_readonly();
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

struct TypedAcr : Accessor {
    Type type;
    explicit constexpr TypedAcr (AcrForm s, Type t, AcrFlags f) :
        Accessor(s, f), type(t)
    { }
};

struct FunctiveAcr : Accessor {
    AccessFunc* access_func;
    explicit constexpr FunctiveAcr (AcrForm s, AccessFunc* a, AcrFlags f) :
        Accessor(s, f), access_func(a)
    { }
};

 // Yes Accessors are comparable!  Two Accessors are the same if they come from
 // the same place in the same AYU_DESCRIBE block, or if they're dynamically
 // generated from the same inputs.  Access capabilities are IGNORED when
 // comparing accessors for equality.
bool operator== (const Accessor&, const Accessor&);
usize hash_acr (const Accessor&);

template <class Acr>
constexpr Acr constexpr_acr (Acr a) {
    a.ref_count = 0;
    return a;
}

///// ACCESSOR TYPES

/// member

template <class From, class To>
struct MemberAcr : TypedAcr {
    using AcrFromType = From;
    using AcrToType = To;
    To From::* mp;
    explicit constexpr MemberAcr (To From::* mp, AcrFlags flags) :
        TypedAcr(AF::Member, Type::For_constexpr<To>(), flags), mp(mp)
    { }
};

/// base

 // Optimization for when base is at the same address as derived
template <class From, class To>
struct NoopAcr : TypedAcr {
    using AcrFromType = From;
    using AcrToType = To;
    explicit constexpr NoopAcr (AcrFlags flags) :
        TypedAcr(AF::Noop, Type::For_constexpr<To>(), flags)
    { }
};

template <class From, class To>
struct BaseAcr : FunctiveAcr {
    using AcrFromType = From;
    using AcrToType = To;
    static void _access (
        const Accessor*, Mu& from, AccessCB cb, AccessCaps
    ) {
         // reinterpret then implicit upcast
        To& to = reinterpret_cast<From&>(from);
        cb(Type::For<To>(), (Mu*)&to);
    }
    explicit constexpr BaseAcr (AcrFlags flags) :
        FunctiveAcr(AF::Functive, &_access, flags)
    { }
};

/// ref_func

 // It's the programmer's responsibility to know whether they're allowed to
 // address the returned reference or not.
template <class From, class To>
struct RefFuncAcr : TypedAcr {
    using AcrFromType = From;
    using AcrToType = To;
    To&(* f )(From&);
    explicit constexpr RefFuncAcr (To&(* f )(From&), AcrFlags flags) :
        TypedAcr(AF::RefFunc, Type::For_constexpr<To>(), flags),
        f(f)
    { }
};

/// const_ref_func
template <class From, class To>
struct ConstRefFuncAcr : TypedAcr {
    using AcrFromType = From;
    using AcrToType = To;
    const To&(* f )(const From&);
    explicit constexpr ConstRefFuncAcr (
        const To&(* f )(const From&), AcrFlags flags
    ) :
        TypedAcr(
            AF::RefFunc, Type::For_constexpr<To>(),
            flags | AcrFlags::Readonly
        ),
        f(f)
    { }
};

/// const_ref_funcs

template <class To>
struct RefFuncsAcr1 : FunctiveAcr {
    using FunctiveAcr::FunctiveAcr;
    static void _access (const Accessor*, Mu&, AccessCB, AccessCaps);
};
template <class From, class To>
struct RefFuncsAcr : RefFuncsAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    const To&(* getter )(const From&);
    void(* setter )(From&, const To&);
    explicit constexpr RefFuncsAcr (
        const To&(* g )(const From&),
        void(* s )(From&, const To&),
        AcrFlags flags
    ) :
        RefFuncsAcr1<To>(
            AF::Functive, &RefFuncsAcr1<To>::_access,
            flags | AcrFlags::Unaddressable
        ),
        getter(g), setter(s)
    { }
};
template <class To>
void RefFuncsAcr1<To>::_access (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps mode
) {
    auto self = static_cast<const RefFuncsAcr<Mu, To>*>(acr);
    To tmp = mode % AC::Read ? self->getter(from) : To();
    cb(Type::For<To>(), (Mu*)&tmp);
    if (mode % AC::Write) self->setter(from, move(tmp));
}

/// value_func

template <class To>
struct ValueFuncAcr1 : FunctiveAcr {
    using FunctiveAcr::FunctiveAcr;
    static void _access (const Accessor*, Mu&, AccessCB, AccessCaps);
};
template <class From, class To>
struct ValueFuncAcr : ValueFuncAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    To(* f )(const From&);
    explicit constexpr ValueFuncAcr (To(* f )(const From&), AcrFlags flags) :
        ValueFuncAcr1<To>(
            AF::Functive, &ValueFuncAcr1<To>::_access,
            flags | AcrFlags::Readonly | AcrFlags::Unaddressable
        ),
        f(f)
    { }
};
template <class To>
void ValueFuncAcr1<To>::_access (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps mode
) {
    expect(mode == AC::Read);
    auto self = static_cast<const ValueFuncAcr<Mu, To>*>(acr);
    const To tmp = self->f(from);
    cb(Type::For<To>(), (Mu*)&tmp);
}

/// value_funcs

template <class To>
struct ValueFuncsAcr1 : FunctiveAcr {
    using FunctiveAcr::FunctiveAcr;
    static void _access (const Accessor*, Mu&, AccessCB, AccessCaps);
};
template <class From, class To>
struct ValueFuncsAcr : ValueFuncsAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    To(* getter )(const From&);
    void(* setter )(From&, To);
    explicit constexpr ValueFuncsAcr (
        To(* g )(const From&),
        void(* s )(From&, To),
        AcrFlags flags
    ) :
        ValueFuncsAcr1<To>(
            AF::Functive, &ValueFuncsAcr1<To>::_access,
            flags | AcrFlags::Unaddressable
        ),
        getter(g), setter(s)
    { }
};
template <class To>
void ValueFuncsAcr1<To>::_access (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps mode
) {
    auto self = static_cast<const ValueFuncsAcr<Mu, To>*>(acr);
    To tmp = mode % AC::Read ? self->getter(from) : To();
    cb(Type::For<To>(), (Mu*)&tmp);
    if (mode % AC::Write) self->setter(from, move(tmp));
}

/// mixed_funcs

template <class To>
struct MixedFuncsAcr1 : FunctiveAcr {
    using FunctiveAcr::FunctiveAcr;
    static void _access (const Accessor*, Mu&, AccessCB, AccessCaps);
};
template <class From, class To>
struct MixedFuncsAcr : MixedFuncsAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    To(* getter )(const From&);
    void(* setter )(From&, const To&);
    explicit constexpr MixedFuncsAcr (
        To(* g )(const From&),
        void(* s )(From&, const To&),
        AcrFlags flags
    ) :
        MixedFuncsAcr1<To>(
            AF::Functive, &MixedFuncsAcr1<To>::_access,
            flags | AcrFlags::Unaddressable
        ),
        getter(g), setter(s)
    { }
};
template <class To>
void MixedFuncsAcr1<To>::_access (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps mode
) {
    auto self = static_cast<const MixedFuncsAcr<Mu, To>*>(acr);
    To tmp = mode % AC::Read ? self->getter(from) : To();
    cb(Type::For<To>(), (Mu*)&tmp);
    if (mode % AC::Write) self->setter(from, move(tmp));
}

/// assignable

template <class From, class To>
struct AssignableAcr : FunctiveAcr {
    using AcrFromType = From;
    using AcrToType = To;
    static void _access (
        const Accessor*, Mu& from_mu, AccessCB cb, AccessCaps mode
    ) {
        From& from = reinterpret_cast<From&>(from_mu);
        To tmp;
        if (mode % AC::Read) tmp = from;
        cb(Type::For<To>(), (Mu*)&tmp);
        if (mode % AC::Write) from = tmp;
        return;
    }
    explicit constexpr AssignableAcr (AcrFlags flags) :
        FunctiveAcr(AF::Functive, &_access, flags | AcrFlags::Unaddressable)
    { }
};

/// variable

template <class From, class To>
struct VariableAcr : TypedAcr {
    using AcrFromType = From;
    using AcrToType = To;
    static_assert(alignof(To) <= alignof(usize));
    alignas(usize) mutable To value;
     // This ACR cannot be constexpr.
     // This ACR cannot be addressable, because then chaining may take the
     // address but then release this ACR object, invalidating the reference.
    explicit VariableAcr (To&& v, AcrFlags flags) :
        TypedAcr(
            AF::Variable, Type::For_constexpr<To>(),
            flags | AcrFlags::Unaddressable
        ),
        value(move(v))
    { }
};

/// constant

template <class From, class To>
struct ConstantAcr : TypedAcr {
    using AcrFromType = From;
    using AcrToType = To;
    static_assert(alignof(To) <= alignof(usize));
    alignas(usize) const To value;  // The offset of this MUST match VariableAcr::value
    explicit constexpr ConstantAcr (const To& v, AcrFlags flags) :
        TypedAcr(
            AF::Variable, Type::For_constexpr<To>(),
            flags | AcrFlags::Readonly | AcrFlags::Unaddressable
        ),
        value(v)
    { }
};

/// constant_pointer

template <class From, class To>
struct ConstantPtrAcr : TypedAcr {
    using AcrFromType = From;
    using AcrToType = To;
    const To* pointer;
    explicit constexpr ConstantPtrAcr (const To* p, AcrFlags flags) :
        TypedAcr(
            AF::ConstantPtr, Type::For_constexpr<To>(),
            flags | AcrFlags::Readonly
        ),
        pointer(p)
    { }
};

/// anyref_func

 // This is a little awkward because we can't transfer the flags from the
 // calculated AnyRef's acr to this one.  We'll just have to hope we don't
 // miss anything important.
template <class From>
struct AnyRefFuncAcr : Accessor {
    using AcrFromType = From;
    AnyRef(* f )(From&);
    explicit constexpr AnyRefFuncAcr (
        AnyRef(* f )(From&), AcrFlags flags
    ) :
        Accessor(AF::AnyRefFunc, flags), f(f)
    { }
};

/// anyptr_func

template <class From>
struct AnyPtrFuncAcr : Accessor {
    using AcrFromType = From;
    AnyPtr(* f )(From&);
    explicit constexpr AnyPtrFuncAcr (
        AnyPtr(* f )(From&), AcrFlags flags
    ) :
        Accessor(AF::AnyPtrFunc, flags), f(f)
    { }
};

} // namespace ayu::in

template <>
struct std::hash<ayu::in::Accessor> {
    size_t operator () (const ayu::in::Accessor& acr) const {
        return ayu::in::hash_acr(acr);
    }
};
