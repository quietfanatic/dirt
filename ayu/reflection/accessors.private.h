#pragma once
#include "accessors.internal.h"
#include "descriptors.internal.h"

namespace ayu::in {

struct ChainAcr : Accessor {
    const Accessor* outer;
    const Accessor* inner;
    explicit ChainAcr (const Accessor* outer, const Accessor* inner) noexcept;
    ~ChainAcr ();
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
     // Theoretically we could define inverse_address for this, but we'll never
     // need it, since this will never be constructed with an addressable a.
    static constexpr AcrVT _vt = {&_access};
};

struct ChainAttrFuncAcr : Accessor {
    const Accessor* outer;
    AttrFunc<Mu>* f;
    AnyString key;
    ChainAttrFuncAcr (const Accessor* o, AttrFunc<Mu>* f, AnyString k) :
        Accessor(&_vt, AS::ChainAttrFunc, o->flags), outer(o), f(f), key(move(k))
    { outer->inc(); }
    ~ChainAttrFuncAcr () { outer->dec(); }
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    static constexpr AcrVT _vt = {&_access};
};

struct ChainElemFuncAcr : Accessor {
    const Accessor* outer;
    ElemFunc<Mu>* f;
    u32 index;
    ChainElemFuncAcr (const Accessor* o, ElemFunc<Mu>* f, u32 i) :
        Accessor(&_vt, AS::ChainElemFunc, o->flags), outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainElemFuncAcr () { outer->dec(); }
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    static constexpr AcrVT _vt = {&_access};
};

struct ChainDataFuncAcr : Accessor {
    const Accessor* outer;
    DataFunc<Mu>* f;
    u32 index;
    ChainDataFuncAcr (const Accessor* o, DataFunc<Mu>* f, u32 i) :
        Accessor(&_vt, AS::ChainDataFunc, o->flags), outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainDataFuncAcr () { outer->dec(); }
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    static constexpr AcrVT _vt = {&_access};
};

} // namespace ayu::in
