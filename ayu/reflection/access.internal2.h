#pragma once
#include "access.internal1.h"
#include "type.h"

namespace ayu::in {

 // TODO: this isn't really necessary at this point
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
struct ReinterpretAcr : TypedAcr {
    using AcrFromType = From;
    using AcrToType = To;
    explicit constexpr ReinterpretAcr (AcrFlags flags) :
        TypedAcr(AF::Reinterpret, Type::For_constexpr<To>(), flags)
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
    if (!(mode % AC::Write)) {
        cb(Type::For<To>(), (Mu*)&self->getter(from));
    }
    else {
        To tmp = mode % AC::Read ? self->getter(from) : To();
        cb(Type::For<To>(), (Mu*)&tmp);
        self->setter(from, move(tmp));
    }
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

 // funcs

template <class From, class Getter, class Setter>
struct FuncsAcr : FunctiveAcr {
    using To =
        std::remove_cvref_t<decltype(
            (*(Getter*)null)(*(From*)null)
        )>;
    using AcrFromType = From;
    using AcrToType = To;
    [[no_unique_address]] Getter getter;
    [[no_unique_address]] Setter setter;
    explicit constexpr FuncsAcr (Getter g, Setter s, AcrFlags flags) :
        FunctiveAcr(
            AF::Functive, &_access,
            flags | AcrFlags::Unaddressable
        ),
        getter(g), setter(s)
    { }
    static void _access (
        const Accessor* acr, Mu& f, AccessCB cb, AccessCaps mode
    ) {
        auto self = static_cast<const FuncsAcr<From, Getter, Setter>*>(acr);
        auto& from = reinterpret_cast<From&>(f);
        if constexpr (
            std::is_trivially_default_constructible_v<To> &&
            std::is_trivially_copy_constructible_v<To>
        ) {
            To tmp;
            if (mode % AC::Read) [[likely]] new (&tmp) To(self->getter(from));
            cb(Type::For<To>(), (Mu*)&tmp);
            if (mode % AC::Write) [[likely]] self->setter(from, move(tmp));
        }
        else {
            if (mode % AC::Read) [[likely]];
            To tmp = mode % AC::Read ? self->getter(from) : To();
            cb(Type::For<To>(), (Mu*)&tmp);
            if (mode % AC::Write) [[likely]] self->setter(from, move(tmp));
        }
    }
};

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

} // ayu::in
