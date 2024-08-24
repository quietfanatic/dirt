#include "accessors.private.h"

#include "describe.h"
#include "descriptors.private.h"
#include "reference.h"

namespace ayu::in {

Type AccessorWithType::_type (const Accessor* acr, Mu*) {
    auto self = static_cast<const AccessorWithType*>(acr);
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

void ConstRefFuncAcr0::_access (
    const Accessor* acr, [[maybe_unused]] AccessMode mode,
    Mu& from, CallbackRef<void(Mu&)> cb
) {
    expect(mode == AccessMode::Read);
    auto self = static_cast<const ConstRefFuncAcr2<Mu, Mu>*>(acr);
    cb(const_cast<Mu&>((self->f)(from)));
}
Mu* ConstRefFuncAcr0::_address (const Accessor* acr, Mu& from) {
    auto self = static_cast<const ConstRefFuncAcr2<Mu, Mu>*>(acr);
    return const_cast<Mu*>(&(self->f)(from));
}

void ConstantPtrAcr0::_access (
    const Accessor* acr, [[maybe_unused]] AccessMode mode,
    Mu&, CallbackRef<void(Mu&)> cb
) {
    expect(mode == AccessMode::Read);
    auto self = static_cast<const ConstantPtrAcr2<Mu, Mu>*>(acr);
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
    return expect(ref.address());
}

ChainAcr::ChainAcr (const Accessor* outer, const Accessor* inner) noexcept :
    Accessor(
        &_vt,
         // Readonly if either accessor is readonly
        ((outer->flags & AcrFlags::Readonly) |
         (inner->flags & AcrFlags::Readonly)) |
         // Pass through addressable if both are PTA
        ((outer->flags & AcrFlags::PassThroughAddressable) &
         (inner->flags & AcrFlags::PassThroughAddressable))
    ), outer(outer), inner(inner)
{ outer->inc(); inner->inc(); }
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
;    // Have to use modify instead of write for the first mode, or other
     // parts of the item will get clobbered.  Hope this isn't necessary
     // very often.
    auto outer_mode = mode == AccessMode::Write ? AccessMode::Modify : mode;
    return self->outer->access(outer_mode, v, [self, mode, cb](Mu& w){
        self->inner->access(mode, w, cb);
    });
}
Mu* ChainAcr::_address (const Accessor* acr, Mu& v) {
    auto self = static_cast<const ChainAcr*>(acr);
    if (!!(self->outer->flags & AcrFlags::PassThroughAddressable)) {
        Mu* r = null;
        self->outer->access(AccessMode::Read, v, [&r, self](Mu& w){
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
void ChainAcr::_destroy (Accessor* acr) noexcept {
    auto self = static_cast<const ChainAcr*>(acr);
    self->inner->dec(); self->outer->dec();
}

Type ChainAttrFuncAcr::_type (const Accessor* acr, Mu* v) {
    auto self = static_cast<const ChainAttrFuncAcr*>(acr);
    if (!v) return Type();
    Type r;
    self->outer->read(*v, [&r, self](Mu& w){
        r = self->f(w, self->key).type();
    });
    return r;
}
void ChainAttrFuncAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const ChainAttrFuncAcr*>(acr);
    auto outer_mode = mode == AccessMode::Write ? AccessMode::Modify : mode;
    self->outer->access(outer_mode, v, [self, mode, cb](Mu& w){
        self->f(w, self->key).access(mode, cb);
    });
}
Mu* ChainAttrFuncAcr::_address (const Accessor* acr, Mu& v) {
    auto self = static_cast<const ChainAttrFuncAcr*>(acr);
    if (!!(self->outer->flags & AcrFlags::PassThroughAddressable)) {
        Mu* r = null;
        self->outer->access(AccessMode::Read, v, [&r, self](Mu& w){
            r = self->f(w, self->key).address();
        });
        return r;
    }
    else if (auto addr = self->outer->address(v)) {
        return self->f(*addr, self->key).address();
    }
    else return null;
}
void ChainAttrFuncAcr::_destroy (Accessor* acr) noexcept {
    auto self = static_cast<const ChainAttrFuncAcr*>(acr);
    self->outer->dec();
    self->~ChainAttrFuncAcr();
}

Type ChainElemFuncAcr::_type (const Accessor* acr, Mu* v) {
    auto self = static_cast<const ChainElemFuncAcr*>(acr);
    if (!v) return Type();
    Type r;
    self->outer->read(*v, [&r, self](Mu& w){
        r = self->f(w, self->index).type();
    });
    return r;
}
void ChainElemFuncAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const ChainElemFuncAcr*>(acr);
    auto outer_mode = mode == AccessMode::Write ? AccessMode::Modify : mode;
    self->outer->access(outer_mode, v, [self, mode, cb](Mu& w){
        self->f(w, self->index).access(mode, cb);
    });
}
Mu* ChainElemFuncAcr::_address (const Accessor* acr, Mu& v) {
    auto self = static_cast<const ChainElemFuncAcr*>(acr);
    if (!!(self->outer->flags & AcrFlags::PassThroughAddressable)) {
        Mu* r = null;
        self->outer->access(AccessMode::Read, v, [&r, self](Mu& w){
            r = self->f(w, self->index).address();
        });
        return r;
    }
    else if (auto addr = self->outer->address(v)) {
        return self->f(*addr, self->index).address();
    }
    else return null;
}
void ChainElemFuncAcr::_destroy (Accessor* acr) noexcept {
    auto self = static_cast<const ChainElemFuncAcr*>(acr);
    self->outer->dec();
    self->~ChainElemFuncAcr();
}

Type ChainDataFuncAcr::_type (const Accessor* acr, Mu* v) {
    auto self = static_cast<const ChainDataFuncAcr*>(acr);
    if (!v) return Type();
    Type r;
    self->outer->read(*v, [&r, self](Mu& w){
        r = self->f(w).type;
    });
    return r;
}
void ChainDataFuncAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, CallbackRef<void(Mu&)> cb
) {
    auto self = static_cast<const ChainDataFuncAcr*>(acr);
    auto outer_mode = mode == AccessMode::Write ? AccessMode::Modify : mode;
    self->outer->access(outer_mode, v, [self, mode, cb](Mu& w){
        AnyPtr data = self->f(w);
        auto desc = DescriptionPrivate::get(data.type);
        cb(*(Mu*)(
            (char*)data.address + self->index * desc->cpp_size
        ));
    });
}
Mu* ChainDataFuncAcr::_address (const Accessor* acr, Mu& v) {
    auto self = static_cast<const ChainDataFuncAcr*>(acr);
    AnyPtr data;
    if (!!(self->outer->flags & AcrFlags::PassThroughAddressable)) {
        self->outer->access(AccessMode::Read, v, [&data, self](Mu& w){
            data = self->f(w);
        });
    }
    else if (auto addr = self->outer->address(v)) {
        data = self->f(*addr);
    }
    else return null;
    auto desc = DescriptionPrivate::get(data.type);
    return (Mu*)((char*)data.address + self->index * desc->cpp_size);
}
void ChainDataFuncAcr::_destroy (Accessor* acr) noexcept {
    auto self = static_cast<const ChainDataFuncAcr*>(acr);
    self->outer->dec();
    self->~ChainDataFuncAcr();
}

 // How do we compare dynamically-typed Accessors without adding an extra
 // "compare" virtual method?  Easy, just examine the vtable pointers.  If you
 // didn't want to know, you shouldn't have asked.
bool operator== (const Accessor& a, const Accessor& b) {
    if (&a == &b) return true;
    if (a.vt != b.vt) return false;
     // These ACRs are dynamically generated, but have a limited set of types,
     // so we can dissect them and compare their members.
    if (a.vt == &ChainAcr::_vt) {
        auto& aa = reinterpret_cast<const ChainAcr&>(a);
        auto& bb = reinterpret_cast<const ChainAcr&>(b);
        return *aa.outer == *bb.outer && *aa.inner == *bb.inner;
    }
    else if (a.vt == &ChainAttrFuncAcr::_vt) {
        auto& aa = reinterpret_cast<const ChainAttrFuncAcr&>(a);
        auto& bb = reinterpret_cast<const ChainAttrFuncAcr&>(b);
        return *aa.outer == *bb.outer && aa.f == bb.f && aa.key == bb.key;
    }
    else if (a.vt == &ChainElemFuncAcr::_vt) {
        auto& aa = reinterpret_cast<const ChainElemFuncAcr&>(a);
        auto& bb = reinterpret_cast<const ChainElemFuncAcr&>(b);
        return *aa.outer == *bb.outer && aa.f == bb.f && aa.index == bb.index;
    }
    else if (a.vt == &ChainDataFuncAcr::_vt) {
        auto& aa = reinterpret_cast<const ChainDataFuncAcr&>(a);
        auto& bb = reinterpret_cast<const ChainDataFuncAcr&>(b);
        return *aa.outer == *bb.outer && aa.f == bb.f && aa.index == bb.index;
    }
     // Other ACRs can have a diverse range of parameterized types, so comparing
     // their contents is not feasible.  Fortunately, they should all be
     // statically generated, so if two ACRs refer to the same member of a type,
     // they should have the same address.
    else return false;
}

usize hash_acr (const Accessor& a) {
    if (a.vt == &ChainAcr::_vt) {
        auto& aa = reinterpret_cast<const ChainAcr&>(a);
        return hash_combine(hash_acr(*aa.outer), hash_acr(*aa.inner));
    }
    else if (a.vt == &ChainAttrFuncAcr::_vt) {
        auto& aa = reinterpret_cast<const ChainAttrFuncAcr&>(a);
        return hash_combine(
            hash_combine(
                hash_acr(*aa.outer),
                std::hash<AttrFunc<Mu>*>{}(aa.f)
            ),
            std::hash<AnyString>{}(aa.key)
        );
    }
    else if (a.vt == &ChainElemFuncAcr::_vt) {
        auto& aa = reinterpret_cast<const ChainElemFuncAcr&>(a);
        return hash_combine(
            hash_combine(
                hash_acr(*aa.outer),
                std::hash<ElemFunc<Mu>*>{}(aa.f)
            ),
            std::hash<usize>{}(aa.index)
        );
    }
    else if (a.vt == &ChainDataFuncAcr::_vt) {
        auto& aa = reinterpret_cast<const ChainDataFuncAcr&>(a);
        return hash_combine(
            hash_combine(
                hash_acr(*aa.outer),
                std::hash<DataFunc<Mu>*>{}(aa.f)
            ),
            std::hash<usize>{}(aa.index)
        );
    }
    else return std::hash<const Accessor*>{}(&a);
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

static tap::TestSet tests ("dirt/ayu/reflection/accessors", []{
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
