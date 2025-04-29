#pragma once
#include "access.internal.h"

namespace ayu::in {

struct ChainAcr : Accessor {
    const Accessor* outer;
    const Accessor* inner;
    explicit ChainAcr (const Accessor* outer, const Accessor* inner) noexcept :
        Accessor(
            AF::Chain,
            outer->caps * inner->caps,
            outer->tree_flags | inner->tree_flags
        ),
        outer(outer), inner(inner)
    { outer->inc(); inner->inc(); }
    ~ChainAcr () { inner->dec(); outer->dec(); }
};

struct ChainAttrFuncAcr : Accessor {
    const Accessor* outer;
    AttrFunc<Mu>* f;
    AnyString key;
    ChainAttrFuncAcr (const Accessor* o, AttrFunc<Mu>* f, AnyString k) :
        Accessor(AF::ChainAttrFunc, o->caps, o->tree_flags),
        outer(o), f(f), key(move(k))
    { outer->inc(); }
    ~ChainAttrFuncAcr () { outer->dec(); }
};

struct ChainElemFuncAcr : Accessor {
    const Accessor* outer;
    ElemFunc<Mu>* f;
    u32 index;
    ChainElemFuncAcr (const Accessor* o, ElemFunc<Mu>* f, u32 i) :
        Accessor(AF::ChainElemFunc, o->caps, o->tree_flags),
        outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainElemFuncAcr () { outer->dec(); }
};

struct ChainDataFuncAcr : Accessor {
    const Accessor* outer;
    DataFunc<Mu>* f;
    u32 index;
    ChainDataFuncAcr (const Accessor* o, DataFunc<Mu>* f, u32 i) :
        Accessor(AF::ChainDataFunc, o->caps, o->tree_flags),
        outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainDataFuncAcr () { outer->dec(); }
};

} // namespace ayu::in
