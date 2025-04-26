#pragma once
#include "access.internal.h"
#include "description.internal.h"

namespace ayu::in {

static inline
AcrFlags chain_acr_flags (AcrFlags o, AcrFlags i) {
    AcrFlags r = {};
     // Readonly if either accessor is readonly
    r |= (o | i) & AcrFlags::Readonly;
     // Pass through addressable if both are PTA
    r |= (o & i) & AcrFlags::PassThroughAddressable;
    if (!!(o & AcrFlags::PassThroughAddressable)) {
         // If outer is pta, unaddressable if inner is unaddressable
        r |= i & AcrFlags::Unaddressable;
    }
    else {
         // Otherwise if either is unaddressable
        r |= (o & i) & AcrFlags::Unaddressable;
    }
    return r;
}

struct ChainAcr : Accessor {
    const Accessor* outer;
    const Accessor* inner;
    explicit ChainAcr (const Accessor* outer, const Accessor* inner) noexcept :
        Accessor(AF::Chain, chain_acr_flags(outer->flags, inner->flags)),
        outer(outer), inner(inner)
    { outer->inc(); inner->inc(); }
    ~ChainAcr () { inner->dec(); outer->dec(); }
};

struct ChainAttrFuncAcr : Accessor {
    const Accessor* outer;
    AttrFunc<Mu>* f;
    AnyString key;
    ChainAttrFuncAcr (const Accessor* o, AttrFunc<Mu>* f, AnyString k) :
        Accessor(AF::ChainAttrFunc, o->flags),
        outer(o), f(f), key(move(k))
    { outer->inc(); }
    ~ChainAttrFuncAcr () { outer->dec(); }
};

struct ChainElemFuncAcr : Accessor {
    const Accessor* outer;
    ElemFunc<Mu>* f;
    u32 index;
    ChainElemFuncAcr (const Accessor* o, ElemFunc<Mu>* f, u32 i) :
        Accessor(AF::ChainElemFunc, o->flags),
        outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainElemFuncAcr () { outer->dec(); }
};

struct ChainDataFuncAcr : Accessor {
    const Accessor* outer;
    DataFunc<Mu>* f;
    u32 index;
    ChainDataFuncAcr (const Accessor* o, DataFunc<Mu>* f, u32 i) :
        Accessor(AF::ChainDataFunc, o->flags),
        outer(o), f(f), index(i)
    { outer->inc(); }
    ~ChainDataFuncAcr () { outer->dec(); }
};

} // namespace ayu::in
