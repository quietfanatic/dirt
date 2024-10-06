#pragma once
#include "accessors.internal.h"
#include "descriptors.internal.h"

namespace ayu::in {

struct ChainAcr : Accessor {
    const Accessor* outer;
    const Accessor* inner;
    explicit ChainAcr (const Accessor* outer, const Accessor* inner) noexcept;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    static void _destroy (Accessor*) noexcept;
     // Theoretically we could define inverse_address for this, but we'll never
     // need it, since this will never be constructed with an addressable a.
    static constexpr AcrVT _vt = {&_access, &_destroy};
};

struct ChainAttrFuncAcr : Accessor {
    const Accessor* outer;
    AttrFunc<Mu>* f;
    AnyString key;
    ChainAttrFuncAcr (const Accessor* o, AttrFunc<Mu>* f, AnyString k) :
        Accessor(&_vt, o->flags), outer(o), f(f), key(move(k))
    { outer->inc(); }
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    static void _destroy (Accessor* acr) noexcept;
    static constexpr AcrVT _vt = {&_access, &_destroy};
};

struct ChainElemFuncAcr : Accessor {
    const Accessor* outer;
    ElemFunc<Mu>* f;
    uint index;
    ChainElemFuncAcr (const Accessor* o, ElemFunc<Mu>* f, uint i) :
        Accessor(&_vt, o->flags), outer(o), f(f), index(i)
    { outer->inc(); }
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    static void _destroy (Accessor* acr) noexcept;
    static constexpr AcrVT _vt = {&_access, &_destroy};
};

struct ChainDataFuncAcr : Accessor {
    const Accessor* outer;
    DataFunc<Mu>* f;
    uint index;
    ChainDataFuncAcr (const Accessor* o, DataFunc<Mu>* f, uint i) :
        Accessor(&_vt, o->flags), outer(o), f(f), index(i)
    { outer->inc(); }
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    static void _destroy (Accessor* acr) noexcept;
    static constexpr AcrVT _vt = {&_access, &_destroy};
};

} // namespace ayu::in
