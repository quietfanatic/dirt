#pragma once
#include "accessors.internal.h"
#include "descriptors.internal.h"

namespace ayu::in {

struct ChainAcr : Accessor {
    const Accessor* outer;
    const Accessor* inner;
    explicit ChainAcr (const Accessor* outer, const Accessor* inner) noexcept;
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor*, Mu&);
    static void _destroy (Accessor*) noexcept;
     // Theoretically we could define inverse_address for this, but we'll never
     // need it, since this will never be constructed with an addressable a.
    static constexpr AcrVT _vt = {&_type, &_access, &_address, null, &_destroy};
};

struct ChainAttrFuncAcr : Accessor {
    const Accessor* outer;
    AttrFunc<Mu>* f;
    AnyString key;
    ChainAttrFuncAcr (const Accessor* o, AttrFunc<Mu>* f, AnyString k) :
        Accessor(&_vt, o->flags), outer(o), f(f), key(move(k))
    { outer->inc(); }
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor* acr, Mu& v);
    static void _destroy (Accessor* acr) noexcept;
    static constexpr AcrVT _vt = {&_type, &_access, &_address, null, &_destroy};
};

struct ChainElemFuncAcr : Accessor {
    const Accessor* outer;
    ElemFunc<Mu>* f;
    usize index;
    ChainElemFuncAcr (const Accessor* o, ElemFunc<Mu>* f, usize i) :
        Accessor(&_vt, o->flags), outer(o), f(f), index(i)
    { outer->inc(); }
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor* acr, Mu& v);
    static void _destroy (Accessor* acr) noexcept;
    static constexpr AcrVT _vt = {&_type, &_access, &_address};
};

struct ChainDataFuncAcr : Accessor {
    const Accessor* outer;
    DataFunc<Mu>* f;
    usize index;
    ChainDataFuncAcr (const Accessor* o, DataFunc<Mu>* f, usize i) :
        Accessor(&_vt, o->flags), outer(o), f(f), index(i)
    { outer->inc(); }
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor* acr, Mu& v);
    static void _destroy (Accessor* acr) noexcept;
    static constexpr AcrVT _vt = {&_type, &_access, &_address};
};

} // namespace ayu::in
