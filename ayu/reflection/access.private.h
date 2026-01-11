#pragma once
#include "access.internal2.h"

namespace ayu::in {

struct ChainAcr : Accessor {
    const Accessor* outer;
    const Accessor* inner;
    explicit ChainAcr (const Accessor* outer, const Accessor* inner, AccessCaps c) noexcept :
        Accessor(AF::Chain, c),
        outer(outer), inner(inner)
    { outer->inc(); inner->inc(); }
    ~ChainAcr () { inner->dec(); outer->dec(); }
};

struct ChainAttrFuncAcr : Accessor {
    const Accessor* outer;
    AttrFunc<Mu>* f;
    AnyString key;
    ChainAttrFuncAcr (const Accessor* o, AttrFunc<Mu>* f, AnyString k, AccessCaps c) :
        Accessor(AF::ChainAttrFunc, c),
        outer(o), f(f), key(move(k))
    { outer->inc(); }
    ~ChainAttrFuncAcr () { outer->dec(); }
};

struct ChainElemFuncAcr : Accessor {
    const Accessor* outer;
    ElemFunc<Mu>* f;
    u32 index;
    ChainElemFuncAcr (const Accessor* o, ElemFunc<Mu>* f, u32 i, AccessCaps c) :
        Accessor(AF::ChainElemFunc, c),
        outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainElemFuncAcr () { outer->dec(); }
};

struct ChainDataFuncAcr : Accessor {
    const Accessor* outer;
    DataFunc<Mu>* f;
    u32 index;
    ChainDataFuncAcr (const Accessor* o, DataFunc<Mu>* f, u32 i, AccessCaps c) :
        Accessor(AF::ChainDataFunc, c),
        outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainDataFuncAcr () { outer->dec(); }
};

} // namespace ayu::in
