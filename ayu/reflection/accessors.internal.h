// This module contains the classes implementing the accessors that can be used
// in AYU_DESCRIBE descriptions.

#pragma once

#include <typeinfo>

#include "../common.h"
#include "../data/tree.h"
#include "type.h"

namespace ayu::in {

///// UNIVERSAL ACCESSOR STUFF

namespace _ {
enum AcrFlags : uint8 {
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
     // Consider this ACR unaddressable even if it normally would be.
    Unaddressable = 0x80,
};
DECLARE_ENUM_BITWISE_OPERATORS(AcrFlags)
} using _::AcrFlags;

 // These belong on AttrDcr and ElemDcr, but we're putting them with the
 // accessor flags to save space.
namespace _ {
enum AttrFlags : uint8 {
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
     // If this is set, map an empty array to the attribute being missing from
     // the object, and an array of one element to the attribute being present
     // with that element as its value.
    CollapseOptional = 0x04,
     // If this is set, the attr will not be serialized in to_tree.
    Invisible = 0x8,
};
DECLARE_ENUM_BITWISE_OPERATORS(AttrFlags)
} using _::AttrFlags;

 // Instead of having separate methods for each type of access, we're using the
 // same method for all of them, and using an enum to differentiate.  This saves
 // a lot of code size, because a lot of ACRs have nearly or exactly the same
 // methods for all access operations.  Even manually demerging identical access
 // methods and storing the same pointer three times in the VT compiles larger
 // than this.
enum class AccessMode {
     // Provides a const ref containing the value of the object.  It may refer
     // to the object itself or to a temporary that will go out of scope when
     // read() returns.
    Read,
     // Provides a ref to which a new value can be written.  It may refer to the
     // object itself, or it may be a reference to a default-constructed
     // temporary.  Neglecting to write to the reference in the callback may
     // clear the object.
    Write,
     // Provides a ref containing the value of the object, to which a new value
     // can be written.  May be implemented as a read followed by a write.
    Modify
};

struct Accessor;

 // Rolling our own vtables.  Using C++'s builtin vtable system pulls in a lot
 // of RTTI information for each class, and when those classes are template
 // instantiations it results in massive code size bloating.
struct AcrVT {
    static Mu* default_address (const Accessor*, Mu&) { return null; }
    static void default_destroy (Accessor*) noexcept { }
    template <class T>
    static Type const_type (const Accessor*, Mu*) {
        return Type::CppType<T>();
    }
     // TODO: merge type and address methods because they're nearly always
     // called together.
    Type(* type )(const Accessor*, Mu*) = null;
    void(* access )(const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>)
        = null;
    Mu*(* address )(const Accessor*, Mu&) = &default_address;
    Mu*(* inverse_address )(const Accessor*, Mu&) = null;
     // Plays role of virtual ~Accessor();
    void(* destroy_this )(Accessor*) noexcept = &default_destroy;
};

 // The base class for all accessors.  Try to keep this small.
struct Accessor {
    static constexpr AcrVT _vt = {};
    const AcrVT* vt = &_vt;
     // If ref_count is 0, this is a constexpr accessor and it can't be
     // modified.  Yes, this does mean that if an accessor accumulates enough
     // references to overflow the count it won't be deleted.  I doubt you'll
     // care.  (Usually refcounts are marked mutable, but that's illegal in
     // constexpr classes.)  Note also that the refcount starts at 1, so when
     // constructing a Reference or a ChainAcr with a new Accessor*, don't call
     // inc() on it.
    uint16 ref_count = 1;
    AcrFlags flags = {};
     // These belong on AttrDcr and ElemDcr but we're storing them here to
     //  save space.
    AttrFlags attr_flags = {};

    explicit constexpr Accessor (const AcrVT* vt, AcrFlags f = {}) :
        vt(vt), flags(f)
    { }

    TreeFlags tree_flags () const {
        return TreeFlags(flags & AcrFlags::AllTreeFlags);
    }

    Type type (Mu* from) const { return vt->type(this, from); }
    void access (AccessMode mode, Mu& from, CallbackRef<void(Mu&)> cb) const {
        expect(mode == AccessMode::Read ||
               mode == AccessMode::Write ||
               mode == AccessMode::Modify
        );
        expect(mode == AccessMode::Read || ~(flags & AcrFlags::Readonly));
        vt->access(this, mode, from, cb);
    }
    void read (Mu& from, CallbackRef<void(Mu&)> cb) const {
        access(AccessMode::Read, from, cb);
    }
    void write (Mu& from, CallbackRef<void(Mu&)> cb) const {
        access(AccessMode::Write, from, cb);
    }
    void modify (Mu& from, CallbackRef<void(Mu&)> cb) const {
        access(AccessMode::Modify, from, cb);
    }
    Mu* address (Mu& from) const {
        if (flags & AcrFlags::Unaddressable) return null;
        return vt->address(this, from);
    }
    Mu* inverse_address (Mu& to) const {
        if (flags & AcrFlags::Unaddressable) return null;
        return vt->inverse_address(this, to);
    }

    void inc () const {
         // Most ACRs are constexpr
        if (ref_count) [[unlikely]] {
            const_cast<uint16&>(ref_count)++;
        }
    }
    NOINLINE void do_dec () const {
        if (!--const_cast<uint16&>(ref_count)) {
            vt->destroy_this(const_cast<Accessor*>(this));
            delete this;
        }
    }
    void dec () const {
        if (ref_count) [[unlikely]] do_dec();
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

 // Merge _type virtual functions into one to improve branch target prediction.
struct AccessorWithType : Accessor {
     // This isn't a plain Type because Type::CppType may not work properly at
     // global init time, and it isn't a std::type_info* because we still need
     // to reference Type::CppType<To> to auto-instantiate template descriptions.
     // We could make this a std::type_info* and manually reference
     // Type::CppType<To> with the comma operator, but if the description for To
     // was declared in the same translation unit, Type::CppType<To>() will be
     // much faster than need_description_for_type_info().
     //
     // Wouldn't it save space to put this in the vtable?  No!  Doing so would
     // require a different vtable for each To type, so it would likely use more
     // space.  TODO: actually test this
    const Description* const* desc;
    static Type _type (const Accessor*, Mu*);
    explicit constexpr AccessorWithType (
        const AcrVT* vt, const Description* const* d, AcrFlags f = {}
    ) : Accessor(vt, f), desc(d)
    { }
};

///// ACCESSOR TYPES

/// base

template <class From, class To>
struct BaseAcr2 : Accessor {
    using AcrFromType = From;
    using AcrToType = To;
    explicit constexpr BaseAcr2 (AcrFlags flags = {}) : Accessor(&_vt, flags) { }
    static void _access (
        const Accessor*, AccessMode, Mu& from, CallbackRef<void(Mu&)> cb
    ) {
        To& to = reinterpret_cast<From&>(from);
        cb(reinterpret_cast<Mu&>(to));
    }
    static Mu* _address (const Accessor*, Mu& from) {
        To& to = reinterpret_cast<From&>(from);
        return &reinterpret_cast<Mu&>(to);
    }
    static Mu* _inverse_address (const Accessor*, Mu& to) {
        From& from = static_cast<From&>(reinterpret_cast<To&>(to));
        return &reinterpret_cast<Mu&>(from);
    }
    static constexpr AcrVT _vt = {
        &AcrVT::const_type<To>, &_access, &_address, &_inverse_address
    };
};

/// member

struct MemberAcr0 : AccessorWithType {
    using AccessorWithType::AccessorWithType;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor*, Mu&);
    static Mu* _inverse_address (const Accessor*, Mu&);
    static constexpr AcrVT _vt = {
        &_type, &_access, &_address, &_inverse_address
    };
};
template <class From, class To>
struct MemberAcr2 : MemberAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    To From::* mp;
    explicit constexpr MemberAcr2 (To From::* mp, AcrFlags flags = {}) :
        MemberAcr0(&_vt, get_indirect_description<To>(), flags), mp(mp)
    { }
};

/// ref_func

struct RefFuncAcr0 : AccessorWithType {
     // It's the programmer's responsibility to know whether they're
     // allowed to address this reference or not.
    using AccessorWithType::AccessorWithType;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor*, Mu&);
    static constexpr AcrVT _vt = {&_type, &_access, &_address};
};
template <class From, class To>
struct RefFuncAcr2 : RefFuncAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    To&(* f )(From&);
    explicit constexpr RefFuncAcr2 (To&(* f )(From&), AcrFlags flags = {}) :
        RefFuncAcr0(&_vt, get_indirect_description<To>(), flags), f(f)
    { }
};

/// const_ref_func

struct ConstRefFuncAcr0 : AccessorWithType {
     // It's the programmer's responsibility to know whether they're
     // allowed to address this reference or not.
    using AccessorWithType::AccessorWithType;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor*, Mu&);
    static constexpr AcrVT _vt = {&_type, &_access, &_address};
};
template <class From, class To>
struct ConstRefFuncAcr2 : ConstRefFuncAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    const To&(* f )(const From&);
    explicit constexpr ConstRefFuncAcr2 (
        const To&(* f )(const From&), AcrFlags flags = {}
    ) :
        ConstRefFuncAcr0(&_vt, get_indirect_description<To>(), flags), f(f)
    { }
};

/// const_ref_funcs

template <class To>
struct RefFuncsAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static constexpr AcrVT _vt = {&AcrVT::const_type<To>, &_access};
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
        RefFuncsAcr1<To>(&RefFuncsAcr1<To>::_vt, flags), getter(g), setter(s)
    { }
};
template <class To>
void RefFuncsAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu& from, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const RefFuncsAcr2<Mu, To>*>(acr);
    switch (mode) {
        case AccessMode::Read: {
            return cb.reinterpret<void(const To&)>()(self->getter(from));
        }
        case AccessMode::Write: {
            To tmp;
            cb.reinterpret<void(To&)>()(tmp);
            return self->setter(from, tmp);
        }
        case AccessMode::Modify: {
            To tmp = self->getter(from);
            cb.reinterpret<void(To&)>()(tmp);
            return self->setter(from, tmp);
        }
        default: never();
    }
}

/// value_func

template <class To>
struct ValueFuncAcr1 : Accessor {
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static constexpr AcrVT _vt = {&AcrVT::const_type<To>, &_access};
    using Accessor::Accessor;
};
template <class From, class To>
struct ValueFuncAcr2 : ValueFuncAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    To(* f )(const From&);
    explicit constexpr ValueFuncAcr2 (To(* f )(const From&), AcrFlags flags = {}) :
        ValueFuncAcr1<To>(&ValueFuncAcr1<To>::_vt, flags | AcrFlags::Readonly),
        f(f)
    { }
};
template <class To>
void ValueFuncAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu& from, CallbackRef<void(Mu&)> cb
) {
    expect(mode == AccessMode::Read);
    auto self = static_cast<const ValueFuncAcr2<Mu, To>*>(acr);
    cb.reinterpret<void(const To&)>()(self->f(from));
}

/// value_funcs

template <class To>
struct ValueFuncsAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static constexpr AcrVT _vt = {&AcrVT::const_type<To>, &_access};
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
        ValueFuncsAcr1<To>(&ValueFuncsAcr1<To>::_vt, flags),
        getter(g), setter(s)
    { }
};
template <class To>
void ValueFuncsAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu& from, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const ValueFuncsAcr2<Mu, To>*>(acr);
    switch (mode) {
        case AccessMode::Read: {
            return cb.reinterpret<void(const To&)>()(self->getter(from));
        }
        case AccessMode::Write: {
            To tmp;
            cb.reinterpret<void(To&)>()(tmp);
            return self->setter(from, move(tmp));
        }
        case AccessMode::Modify: {
            To tmp = self->getter(from);
            cb.reinterpret<void(To&)>()(tmp);
            return self->setter(from, move(tmp));
        }
        default: never();
    }
}

/// mixed_funcs

template <class To>
struct MixedFuncsAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static constexpr AcrVT _vt = {&AcrVT::const_type<To>, &_access};
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
        MixedFuncsAcr1<To>(&MixedFuncsAcr1<To>::_vt, flags),
        getter(g), setter(s)
    { }
};
template <class To>
void MixedFuncsAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu& from, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const MixedFuncsAcr2<Mu, To>*>(acr);
    switch (mode) {
        case AccessMode::Read: {
            return cb.reinterpret<void(const To&)>()(self->getter(from));
        }
        case AccessMode::Write: {
            To tmp;
            cb.reinterpret<void(To&)>()(tmp);
            return self->setter(from, move(tmp));
        }
        case AccessMode::Modify: {
            To tmp = (self->getter)(from);
            cb.reinterpret<void(To&)>()(tmp);
            return self->setter(from, move(tmp));
        }
        default: never();
    }
}

/// assignable

template <class From, class To>
struct AssignableAcr2 : Accessor {
    using AcrFromType = From;
    using AcrToType = To;
    explicit constexpr AssignableAcr2 (AcrFlags flags = {}) :
        Accessor(&_vt, flags)
    { }
    static void _access (
        const Accessor*, AccessMode mode, Mu& from_mu, CallbackRef<void(Mu&)> cb_mu
    ) {
        From& from = reinterpret_cast<From&>(from_mu);
        auto cb = cb_mu.reinterpret<void(To&)>();
        To tmp;
        if (mode != AccessMode::Write) tmp = from;
        cb(tmp);
        if (mode != AccessMode::Read) from = tmp;
        return;
    }
    static constexpr AcrVT _vt = {&AcrVT::const_type<To>, &_access};
};

/// variable

template <class To>
struct VariableAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
     // This ACR cannot be addressable, because then Reference::chain and co.
     //  may take the address of value but then release this ACR object,
     //  invalidating value.
    static void _destroy (Accessor*) noexcept;
    static constexpr AcrVT _vt = {
        &AcrVT::const_type<To>, &_access, &AcrVT::default_address,
        null, &_destroy
    };
};
template <class From, class To>
struct VariableAcr2 : VariableAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    mutable To value;
     // This ACR cannot be constexpr.
    explicit VariableAcr2 (To&& v, AcrFlags flags = {}) :
        VariableAcr1<To>(&VariableAcr1<To>::_vt, flags), value(move(v))
    { }
};
template <class To>
void VariableAcr1<To>::_access (
    const Accessor* acr, AccessMode, Mu&, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const VariableAcr2<Mu, To>*>(acr);
    cb(reinterpret_cast<Mu&>(self->value));
}
template <class To>
void VariableAcr1<To>::_destroy (Accessor* acr) noexcept {
    auto self = static_cast<const VariableAcr2<Mu, To>*>(acr);
    self->~VariableAcr2<Mu, To>();
}

/// constant

template <class To>
struct ConstantAcr1 : Accessor {
    using Accessor::Accessor;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static void _destroy (Accessor*) noexcept;
    static constexpr AcrVT _vt = {
        &AcrVT::const_type<To>, &_access, &AcrVT::default_address,
        null, &_destroy
    };
};
template <class From, class To>
struct ConstantAcr2 : ConstantAcr1<To> {
    using AcrFromType = From;
    using AcrToType = To;
    const To value;
    explicit constexpr ConstantAcr2 (const To& v, AcrFlags flags = {}) :
        ConstantAcr1<To>(&ConstantAcr1<To>::_vt, flags | AcrFlags::Readonly),
        value(v)
    { }
};
template <class To>
void ConstantAcr1<To>::_access (
    const Accessor* acr, AccessMode mode, Mu&, CallbackRef<void(Mu&)> cb
) {
    expect(mode == AccessMode::Read);
    auto self = static_cast<const ConstantAcr2<Mu, To>*>(acr);
    cb(reinterpret_cast<Mu&>(const_cast<To&>(self->value)));
}
template <class To>
void ConstantAcr1<To>::_destroy (Accessor* acr) noexcept {
    auto self = static_cast<const ConstantAcr2<Mu, To>*>(acr);
    self->~ConstantAcr2<Mu, To>();
}

/// constant_pointer

struct ConstantPointerAcr0 : AccessorWithType {
    using AccessorWithType::AccessorWithType;
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
     // Should be okay addressing this.
    static Mu* _address (const Accessor*, Mu&);
    static constexpr AcrVT _vt = {&_type, &_access, &_address};
};

template <class From, class To>
struct ConstantPointerAcr2 : ConstantPointerAcr0 {
    using AcrFromType = From;
    using AcrToType = To;
    const To* pointer;
    explicit constexpr ConstantPointerAcr2 (const To* p, AcrFlags flags = {}) :
        ConstantPointerAcr0(
            &_vt, get_indirect_description<To>(), flags | AcrFlags::Readonly
        ), pointer(p)
    { }
};

/// reference_func

 // This is a little awkward because we can't transfer the flags from the
 // calculated Reference's acr to this one.  We'll just have to hope we don't
 // miss anything important.
struct ReferenceFuncAcr1 : Accessor {
    using Accessor::Accessor;
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor*, Mu&);
    static constexpr AcrVT _vt = {&_type, &_access, &_address};
};
template <class From>
struct ReferenceFuncAcr2 : ReferenceFuncAcr1 {
    using AcrFromType = From;
    using AcrToType = Reference;
    Reference(* f )(From&);
    explicit constexpr ReferenceFuncAcr2 (
        Reference(* f )(From&), AcrFlags flags = {}
    ) :
        ReferenceFuncAcr1(&_vt, flags), f(f)
    { }
};

} // namespace ayu::in

template <>
struct std::hash<ayu::in::Accessor> {
    size_t operator () (const ayu::in::Accessor& acr) const {
        return ayu::in::hash_acr(acr);
    }
};
