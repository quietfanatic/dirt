#pragma once
#include "../internal/accessors-internal.h"

namespace ayu::in {

struct ChainAcr : Accessor {
    const Accessor* outer;
    const Accessor* inner;
    explicit ChainAcr (const Accessor* outer, const Accessor* inner);
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor*, Mu&);
    static void _destroy (Accessor*);
     // Theoretically we could define inverse_address for this, but we'll never
     // need it, since this will never be constructed with an addressable a.
    static constexpr AccessorVT _vt = {
        &_type, &_access, &_address, null, &_destroy
    };
};

struct AttrFuncAcr : Accessor {
    Reference(* fp )(Mu&, AnyString);
    AnyString key;
    AttrFuncAcr (Reference(* fp )(Mu&, AnyString), AnyString k) :
        Accessor(&_vt), fp(fp), key(move(k))
    { }
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor* acr, Mu& v);
    static void _destroy (Accessor* acr);
    static constexpr AccessorVT _vt = {
        &_type, &_access, &_address, null, &_destroy
    };
};

struct ElemFuncAcr : Accessor {
    Reference(* fp )(Mu&, usize);
    size_t index;
    ElemFuncAcr (Reference(* fp )(Mu&, usize), usize i) :
        Accessor(&_vt), fp(fp), index(i)
    { }
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor* acr, Mu& v);
    static constexpr AccessorVT _vt = {&_type, &_access, &_address};
};

} // namespace ayu::in
