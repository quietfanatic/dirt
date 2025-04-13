#pragma once
#include "accessors.internal.h"
#include "description.internal.h"

namespace ayu::in {

struct ChainAcr : Accessor {
    const Accessor* outer;
    const Accessor* inner;
    explicit ChainAcr (const Accessor* outer, const Accessor* inner) noexcept;
    ~ChainAcr ();
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
};

struct ChainAttrFuncAcr : Accessor {
    const Accessor* outer;
    AttrFunc<Mu>* f;
    AnyString key;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    ChainAttrFuncAcr (const Accessor* o, AttrFunc<Mu>* f, AnyString k) :
        Accessor(AF::ChainAttrFunc, &_access, o->flags),
        outer(o), f(f), key(move(k))
    { outer->inc(); }
    ~ChainAttrFuncAcr () { outer->dec(); }
};

struct ChainElemFuncAcr : Accessor {
    const Accessor* outer;
    ElemFunc<Mu>* f;
    u32 index;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    ChainElemFuncAcr (const Accessor* o, ElemFunc<Mu>* f, u32 i) :
        Accessor(AF::ChainElemFunc, &_access, o->flags),
        outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainElemFuncAcr () { outer->dec(); }
};

struct ChainDataFuncAcr : Accessor {
    const Accessor* outer;
    DataFunc<Mu>* f;
    u32 index;
    static void _access (const Accessor*, AccessMode, Mu&, AccessCB);
    ChainDataFuncAcr (const Accessor* o, DataFunc<Mu>* f, u32 i) :
        Accessor(AF::ChainDataFunc, &_access, o->flags),
        outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainDataFuncAcr () { outer->dec(); }
};

} // namespace ayu::in
