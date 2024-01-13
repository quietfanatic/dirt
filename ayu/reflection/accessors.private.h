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

 // TODO: these are always used as the inner acr in ChainAcr, so combine them so
 // only one allocation needs to be made instead of two.
struct AttrFuncAcr : Accessor {
    AttrFunc<Mu>* f;
    AnyString key;
    AttrFuncAcr (AttrFunc<Mu>* f, AnyString k) :
        Accessor(&_vt), f(f), key(move(k))
    { }
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor* acr, Mu& v);
    static void _destroy (Accessor* acr) noexcept;
    static constexpr AcrVT _vt = {&_type, &_access, &_address, null, &_destroy};
};

struct ElemFuncAcr : Accessor {
    ElemFunc<Mu>* f;
    usize index;
    ElemFuncAcr (ElemFunc<Mu>* f, usize i) :
        Accessor(&_vt), f(f), index(i)
    { }
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor* acr, Mu& v);
    static constexpr AcrVT _vt = {&_type, &_access, &_address};
};

struct DataFuncAcr : Accessor {
    DataFunc<Mu>* f;
    usize index;
    DataFuncAcr (DataFunc<Mu>* f, usize i) :
        Accessor(&_vt), f(f), index(i)
    { }
    static Type _type (const Accessor*, Mu*);
    static void _access (const Accessor*, AccessMode, Mu&, CallbackRef<void(Mu&)>);
    static Mu* _address (const Accessor* acr, Mu& v);
    static constexpr AcrVT _vt = {&_type, &_access, &_address};
};

} // namespace ayu::in
