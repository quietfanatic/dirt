#include "accessors-private.h"

#include "../describe.h"
#include "../reference.h"

namespace ayu::in {

Type MemberAcr0::_type (const Accessor* acr, Mu*) {
    auto self = static_cast<const MemberAcr2<Mu, Mu>*>(acr);
    return *self->desc;
}
void MemberAcr0::_access (
    const Accessor* acr, AccessMode, Mu& from, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const MemberAcr2<Mu, Mu>*>(acr);
    cb(from.*(self->mp));
}
Mu* MemberAcr0::_address (const Accessor* acr, Mu& from) {
    auto self = static_cast<const MemberAcr2<Mu, Mu>*>(acr);
    return &(from.*(self->mp));
}
Mu* MemberAcr0::_inverse_address (const Accessor* acr, Mu& to) {
    auto self = static_cast<const MemberAcr2<Mu, Mu>*>(acr);
     // Figure out how much the pointer-to-member changes the address, and
     // subtract that amount instead of adding it.
     // I'm sure this breaks all sorts of rules but it works (crossed fingers).
    char* to_address = reinterpret_cast<char*>(&to);
    isize offset = reinterpret_cast<char*>(&(to.*(self->mp))) - to_address;
    return reinterpret_cast<Mu*>(to_address - offset);
}

Type RefFuncAcr0::_type (const Accessor* acr, Mu*) {
    auto self = static_cast<const RefFuncAcr2<Mu, Mu>*>(acr);
    return *self->desc;
}
void RefFuncAcr0::_access (
    const Accessor* acr, AccessMode, Mu& from, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const RefFuncAcr2<Mu, Mu>*>(acr);
    cb((self->f)(from));
}
Mu* RefFuncAcr0::_address (const Accessor* acr, Mu& from) {
     // It's the programmer's responsibility to know whether they're
     // allowed to do this or not.
    auto self = static_cast<const RefFuncAcr2<Mu, Mu>*>(acr);
    return &(self->f)(from);
}

Type ConstRefFuncAcr0::_type (const Accessor* acr, Mu*) {
    auto self = static_cast<const ConstRefFuncAcr2<Mu, Mu>*>(acr);
    return *self->desc;
}
void ConstRefFuncAcr0::_access (
    const Accessor* acr, [[maybe_unused]] AccessMode mode,
    Mu& from, CallbackRef<void(Mu&)> cb
) {
    expect(mode == ACR_READ);
    auto self = static_cast<const ConstRefFuncAcr2<Mu, Mu>*>(acr);
    cb(const_cast<Mu&>((self->f)(from)));
}
Mu* ConstRefFuncAcr0::_address (const Accessor* acr, Mu& from) {
    auto self = static_cast<const ConstRefFuncAcr2<Mu, Mu>*>(acr);
    return const_cast<Mu*>(&(self->f)(from));
}

Type ConstantPointerAcr0::_type (const Accessor* acr, Mu*) {
    auto self = static_cast<const ConstantPointerAcr2<Mu, Mu>*>(acr);
    return *self->desc;
}
void ConstantPointerAcr0::_access (
    const Accessor* acr, [[maybe_unused]] AccessMode mode,
    Mu&, CallbackRef<void(Mu&)> cb
) {
    expect(mode == ACR_READ);
    auto self = static_cast<const ConstantPointerAcr2<Mu, Mu>*>(acr);
    cb(*const_cast<Mu*>(self->pointer));
}

Type ReferenceFuncAcr1::_type (const Accessor* acr, Mu* from) {
    if (!from) return Type();
    auto self = static_cast<const ReferenceFuncAcr2<Mu>*>(acr);
    return self->f(*from).type();
}
void ReferenceFuncAcr1::_access (
    const Accessor* acr, AccessMode mode, Mu& from, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const ReferenceFuncAcr2<Mu>*>(acr);
     // This will null deref if f returns an empty Reference
    self->f(from).access(mode, cb);
}
Mu* ReferenceFuncAcr1::_address (const Accessor* acr, Mu& from) {
    auto self = static_cast<const ReferenceFuncAcr2<Mu>*>(acr);
    auto ref = self->f(from);
    expect(ref.type());
    return ref.address();
}

ChainAcr::ChainAcr (const Accessor* outer, const Accessor* inner) :
    Accessor(
        &_vt,
         // Readonly if either accessor is readonly
        ((outer->accessor_flags & ACR_READONLY) |
         (inner->accessor_flags & ACR_READONLY)) |
         // Pass through addressable if both are PTA
        ((outer->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE) &
         (inner->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE))
    ), outer(outer), inner(inner)
{
    outer->inc(); inner->inc();
}
Type ChainAcr::_type (const Accessor* acr, Mu* v) {
    auto self = static_cast<const ChainAcr*>(acr);
     // Most accessors ignore the parameter, so we can usually skip the
     // read operation on a.
    Type r = self->inner->type(null);
    if (!r) {
        if (!v) return Type();
        self->outer->read(*v, [&r, self](Mu& w){
            r = self->inner->type(&w);
        });
    }
    return r;
}
void ChainAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const ChainAcr*>(acr);
    switch (mode) {
         // TODO: simplify?
        case ACR_READ: {
            return self->outer->access(ACR_READ, v, [self, cb](Mu& w){
                self->inner->access(ACR_READ, w, cb);
            });
        }
        case ACR_WRITE: {
             // Have to use modify instead of write here, or other parts of the item
             // will get clobbered.  Hope that we don't go down this code path a lot.
            return self->outer->access(ACR_MODIFY, v, [self, cb](Mu& w){
                self->inner->access(ACR_WRITE, w, cb);
            });
        }
        case ACR_MODIFY: {
            return self->outer->access(ACR_MODIFY, v, [self, cb](Mu& w){
                self->inner->access(ACR_MODIFY, w, cb);
            });
        }
    }
}
Mu* ChainAcr::_address (const Accessor* acr, Mu& v) {
    auto self = static_cast<const ChainAcr*>(acr);
    if (self->outer->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE) {
        Mu* r = null;
        self->outer->access(ACR_READ, v, [&r, self](Mu& w){
            r = self->inner->address(w);
        });
        return r;
    }
    else if (auto addr = self->outer->address(v)) {
         // We shouldn't get to this codepath but here it is anyway
        return self->inner->address(*addr);
    }
    else return null;
}
void ChainAcr::_destroy (Accessor* acr) {
    auto self = static_cast<const ChainAcr*>(acr);
    self->inner->dec(); self->outer->dec();
}

Type AttrFuncAcr::_type (const Accessor* acr, Mu* v) {
    if (!v) return Type();
    auto self = static_cast<const AttrFuncAcr*>(acr);
    return self->fp(*v, self->key).type();
}
void AttrFuncAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const AttrFuncAcr*>(acr);
    self->fp(v, self->key).access(mode, cb);
}
Mu* AttrFuncAcr::_address (const Accessor* acr, Mu& v) {
    auto self = static_cast<const AttrFuncAcr*>(acr);
    return self->fp(v, self->key).address();
}
void AttrFuncAcr::_destroy (Accessor* acr) {
    auto self = static_cast<const AttrFuncAcr*>(acr);
    self->~AttrFuncAcr();
}

Type ElemFuncAcr::_type (const Accessor* acr, Mu* v) {
    if (!v) return Type();
    auto self = static_cast<const ElemFuncAcr*>(acr);
    return self->fp(*v, self->index).type();
}
void ElemFuncAcr::_access (const Accessor* acr, AccessMode mode, Mu& v, CallbackRef<void(Mu&)> cb) {
    auto self = static_cast<const ElemFuncAcr*>(acr);
    self->fp(v, self->index).access(mode, cb);
}
Mu* ElemFuncAcr::_address (const Accessor* acr, Mu& v) {
    auto self = static_cast<const ElemFuncAcr*>(acr);
    return self->fp(v, self->index).address();
}

} using namespace ayu::in;
using namespace ayu;

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

namespace ayu::in {
     // For making sure deduction works.  Won't bother making this for other Acrs.
    template <class From, class To>
    MemberAcr2<From, To> deduce_MemberAcr (To From::* mp) {
        return MemberAcr2<From, To>{mp};
    }
}

namespace ayu::test {
    struct Thing {
        int a;
        int b;
    };
    struct SubThing : Thing {
        int c;
    };
} using namespace ayu::test;

 // Don't actually need any description, we just need these to be usable with
 // AYU
AYU_DESCRIBE(ayu::test::Thing)
AYU_DESCRIBE(ayu::test::SubThing)

static tap::TestSet tests ("dirt/ayu/accessors", []{
    using namespace tap;
    SubThing thing2 {7, 8, 9};

    BaseAcr2<SubThing, Thing>{}.read(reinterpret_cast<Mu&>(thing2), [&](Mu& thing){
        is(reinterpret_cast<const Thing&>(thing).b, 8, "BaseAcr::read");
    });
    BaseAcr2<SubThing, Thing>{}.write(reinterpret_cast<Mu&>(thing2), [&](Mu& thing){
        auto& th = reinterpret_cast<Thing&>(thing);
        th.a = 77;
        th.b = 88;
    });
    is(thing2.b, 88, "BaseAcr::write");

    auto test_addressable = [&](Str type, auto acr){
        Thing t {1, 2};
        is(
            acr.address(reinterpret_cast<Mu&>(t)),
            reinterpret_cast<Mu*>(&t.b),
            cat(type, "::address").c_str()
        );
        acr.read(reinterpret_cast<Mu&>(t), [&](Mu& v){
            is(reinterpret_cast<const int&>(v), 2, cat(type, "::read").c_str());
        });
        acr.write(reinterpret_cast<Mu&>(t), [&](Mu& v){
            reinterpret_cast<int&>(v) = 4;
        });
        is(t.b, 4, cat(type, "::write").c_str());
        acr.modify(reinterpret_cast<Mu&>(t), [&](Mu& v){
            reinterpret_cast<int&>(v) += 5;
        });
        is(t.b, 9, cat(type, "::modify").c_str());
    };
    auto test_unaddressable = [&](Str type, auto acr){
        Thing t {1, 2};
        is(
            acr.address(reinterpret_cast<Mu&>(t)),
            null,
            cat(type, "::address return null").c_str()
        );
        acr.read(reinterpret_cast<Mu&>(t), [&](Mu& v){
            is(reinterpret_cast<const int&>(v), 2, cat(type, "::read").c_str());
        });
        acr.write(reinterpret_cast<Mu&>(t), [&](Mu& v){
            reinterpret_cast<int&>(v) = 4;
        });
        is(t.b, 4, cat(type, "::write").c_str());
        acr.modify(reinterpret_cast<Mu&>(t), [&](Mu& v){
            reinterpret_cast<int&>(v) += 5;
        });
        is(t.b, 9, cat(type, "::modify").c_str());
    };

    test_addressable("MemberAcr", deduce_MemberAcr(&Thing::b));
    test_addressable("RefFuncAcr", RefFuncAcr2<Thing, int>{
        [](Thing& t)->int&{ return t.b; }
    });
    test_unaddressable("RefFuncsAcr", RefFuncsAcr2<Thing, int>{
        [](const Thing& t)->const int&{ return t.b; },
        [](Thing& t, const int& v){ t.b = v; }
    });
    test_unaddressable("ValueFuncsAcr", ValueFuncsAcr2<Thing, int>{
        [](const Thing& t)->int{ return t.b; },
        [](Thing& t, int v){ t.b = v; }
    });
    test_unaddressable("MixedFuncsAcr", MixedFuncsAcr2<Thing, int>{
        [](const Thing& t)->int{ return t.b; },
        [](Thing& t, const int& v){ t.b = v; }
    });
    done_testing();
});
#endif
