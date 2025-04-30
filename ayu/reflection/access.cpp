#include "access.private.h"
#include "anyref.h"

namespace ayu::in {

void access_Functive (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps mode
) {
    auto self = static_cast<const FunctiveAcr*>(acr);
    self->access_func(acr, from, cb, mode);
}

NOINLINE
void access_Typed (
    const Accessor* acr, Mu& to, AccessCB cb, AccessCaps
) {
    auto self = static_cast<const TypedAcr*>(acr);
    cb(self->type, &to);
}

void access_Member (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps mode
) {
    auto self = static_cast<const MemberAcr<Mu, Mu>*>(acr);
    access_Typed(acr, from.*(self->mp), cb, mode);
}

void access_RefFunc (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps
) {
    auto self = static_cast<const RefFuncAcr<Mu, Mu>*>(acr);
    Mu& to = self->f(from);
    cb(self->type, &to);
}

void access_ConstantPtr (
    const Accessor* acr, Mu&, AccessCB cb, AccessCaps mode
) {
    auto self = static_cast<const ConstantPtrAcr<Mu, Mu>*>(acr);
    access_Typed(acr, *const_cast<Mu*>(self->pointer), cb, mode);
}

void access_AnyRefFunc (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps mode
) {
    auto self = static_cast<const AnyRefFuncAcr<Mu>*>(acr);
    auto ref = self->f(from);
    self->f(from).access(mode, cb);
}

void access_AnyPtrFunc (
    const Accessor* acr, Mu& from, AccessCB cb, AccessCaps mode
) {
    auto self = static_cast<const AnyPtrFuncAcr<Mu>*>(acr);
    auto ptr = self->f(from);
    if (mode % AC::Write && ptr.readonly()) {
        raise(e_WriteReadonly, "Non-readonly anyptr_func returned readonly AnyPtr.");
    }
    cb(ptr.type(), ptr.address);
}

void access_Variable (
    const Accessor* acr, Mu&, AccessCB cb, AccessCaps mode
) {
     // Can't instantiate this with To=Mu, because it has a To embedded in it.
    auto self = static_cast<const VariableAcr<Mu, usize>*>(acr);
    access_Typed(acr, *(Mu*)&self->value, cb, mode);
}

void access_Chain (
    const Accessor* acr, Mu& ov, AccessCB cb, AccessCaps mode
) {
    struct Frame {
        const ChainAcr* self;
        AccessCB cb;
        AccessCaps mode;
    };
    Frame frame {static_cast<const ChainAcr*>(acr), cb, mode};
     // Have to use modify instead of write for the first mode, or other
     // parts of the item will get clobbered.  Hope this isn't necessary
     // very often.
    auto outer_mode = mode | AC::Read;
    return frame.self->outer->access(outer_mode, ov,
        AccessCB(frame, [](Frame& frame, Type, Mu* iv){
            frame.self->inner->access(frame.mode, *iv, frame.cb);
        })
    );
}

void access_ChainAttrFunc (
    const Accessor* acr, Mu& ov, AccessCB cb, AccessCaps mode
) {
    struct Frame {
        const ChainAttrFuncAcr* self;
        AccessCB cb;
        AccessCaps mode;
    };
    Frame frame {static_cast<const ChainAttrFuncAcr*>(acr), cb, mode};
    auto outer_mode = mode | AC::Read;
    frame.self->outer->access(outer_mode, ov,
        AccessCB(frame, [](Frame& frame, Type, Mu* iv){
            AnyRef inter = frame.self->f(*iv, frame.self->key);
             // TODO: expect caps are same as they were before
            inter.access(frame.mode, frame.cb);
        })
    );
}

void access_ChainElemFunc (
    const Accessor* acr, Mu& ov, AccessCB cb, AccessCaps mode
) {
    struct Frame {
        const ChainElemFuncAcr* self;
        AccessCB cb;
        AccessCaps mode;
    };
    Frame frame {static_cast<const ChainElemFuncAcr*>(acr), cb, mode};
    auto outer_mode = mode | AC::Read;
    frame.self->outer->access(outer_mode, ov,
        AccessCB(frame, [](Frame& frame, Type, Mu* iv){
            AnyRef inter = frame.self->f(*iv, frame.self->index);
            inter.access(frame.mode, frame.cb);
        })
    );
}

void access_ChainDataFunc (
    const Accessor* acr, Mu& ov, AccessCB cb, AccessCaps mode
) {
    struct Frame {
        const ChainDataFuncAcr* self;
        AccessCB cb;
         // We only need this in debug builds but it's not worth doing a bunch
         // of ifdefs to save one word of stack space.
        AccessCaps mode;
    };
    Frame frame {static_cast<const ChainDataFuncAcr*>(acr), cb, mode};
    auto outer_mode = mode | AC::Read;
    frame.self->outer->access(outer_mode, ov,
        AccessCB(frame, [](Frame& frame, Type, Mu* iv){
            AnyPtr p = frame.self->f(*iv);
             // We should already have done bounds checking.  Unfortunately we
             // can't reverify it in debug mode because we've lost the info
             // necessary to get the length.
            p.address = (Mu*)(
                (char*)p.address + frame.self->index * p.type().cpp_size()
            );
            frame.cb(p.type(), p.address);
        })
    );
}

NOINLINE
void delete_Accessor (Accessor* acr) noexcept {
    switch (acr->form) {
        case AF::Variable: {
             // Can't use Mu because it doesn't have a size.  We don't *need* a
             // size, but C++ does in order to do this operation.  So I guess
             // use usize instead?  Hope the alignment works out!
            auto* self = static_cast<VariableAcr<Mu, usize>*>(acr);
            dynamic_destroy(self->type, (Mu*)&self->value);
            break;
        }
        case AF::Chain: {
            auto* self = static_cast<ChainAcr*>(acr);
            self->~ChainAcr();
            break;
        }
        case AF::ChainAttrFunc: {
            auto* self = static_cast<ChainAttrFuncAcr*>(acr);
            self->~ChainAttrFuncAcr();
            break;
        }
        case AF::ChainElemFunc: {
            auto* self = static_cast<ChainElemFuncAcr*>(acr);
            self->~ChainElemFuncAcr();
            break;
        }
        case AF::ChainDataFunc: {
            auto* self = static_cast<ChainDataFuncAcr*>(acr);
            self->~ChainDataFuncAcr();
            break;
        }
        default: break;
    }
    delete acr;
}

NOINLINE
bool operator== (const Accessor& a, const Accessor& b) {
    if (&a == &b) return true;
    if (a.form != b.form) return false;
     // These ACRs are dynamically generated, but have a limited set of types,
     // so we can dissect them and compare their members.
    switch (a.form) {
        case AF::Chain: {
            auto& aa = reinterpret_cast<const ChainAcr&>(a);
            auto& bb = reinterpret_cast<const ChainAcr&>(b);
            return *aa.outer == *bb.outer && *aa.inner == *bb.inner;
        }
        case AF::ChainAttrFunc: {
            auto& aa = reinterpret_cast<const ChainAttrFuncAcr&>(a);
            auto& bb = reinterpret_cast<const ChainAttrFuncAcr&>(b);
            return *aa.outer == *bb.outer && aa.f == bb.f && aa.key == bb.key;
        }
        case AF::ChainElemFunc:  {
            auto& aa = reinterpret_cast<const ChainElemFuncAcr&>(a);
            auto& bb = reinterpret_cast<const ChainElemFuncAcr&>(b);
            return *aa.outer == *bb.outer && aa.f == bb.f && aa.index == bb.index;
        }
        case AF::ChainDataFunc:  {
            auto& aa = reinterpret_cast<const ChainDataFuncAcr&>(a);
            auto& bb = reinterpret_cast<const ChainDataFuncAcr&>(b);
            return *aa.outer == *bb.outer && aa.f == bb.f && aa.index == bb.index;
        }
         // Other ACRs can have a diverse range of parameterized types, so
         // comparing their contents is not feasible.  Fortunately, they should
         // mostly be statically generated, so if two ACRs refer to the same
         // member of a type, they should have the same address.  TODO: We now
         // have more type information, so we can compare more types of
         // accessors.
        default: return false;
    }
}

NOINLINE
usize hash_acr (const Accessor& a) {
    switch (a.form) {
        case AF::Chain: {
            auto& aa = reinterpret_cast<const ChainAcr&>(a);
            return hash_combine(hash_acr(*aa.outer), hash_acr(*aa.inner));
        }
        case AF::ChainAttrFunc:  {
            auto& aa = reinterpret_cast<const ChainAttrFuncAcr&>(a);
            return hash_combine(
                hash_combine(
                    hash_acr(*aa.outer),
                    std::hash<AttrFunc<Mu>*>{}(aa.f)
                ),
                std::hash<AnyString>{}(aa.key)
            );
        }
        case AF::ChainElemFunc:  {
            auto& aa = reinterpret_cast<const ChainElemFuncAcr&>(a);
            return hash_combine(
                hash_combine(
                    hash_acr(*aa.outer),
                    std::hash<ElemFunc<Mu>*>{}(aa.f)
                ),
                std::hash<usize>{}(aa.index)
            );
        }
        case AF::ChainDataFunc:  {
            auto& aa = reinterpret_cast<const ChainDataFuncAcr&>(a);
            return hash_combine(
                hash_combine(
                    hash_acr(*aa.outer),
                    std::hash<DataFunc<Mu>*>{}(aa.f)
                ),
                std::hash<usize>{}(aa.index)
            );
        }
        default: return std::hash<const Accessor*>{}(&a);
    }
}

} using namespace ayu::in;
using namespace ayu;

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"
#include "describe.h"
#include "description.private.h"

namespace ayu::in {
     // For making sure deduction works.  Won't bother making this for other Acrs.
    template <class From, class To>
    MemberAcr<From, To> deduce_MemberAcr (To From::* mp) {
        return MemberAcr<From, To>{mp, {}};
    }
}

namespace ayu::test {
    struct Thing {
        int a;
        int b;
    };
    struct Thinger {
        int d;
    };
    struct SubThing : Thing, Thinger {
        int c;
    };
} using namespace ayu::test;

 // Don't actually need any description, we just need these to be usable with
 // AYU
AYU_DESCRIBE(ayu::test::Thing)
AYU_DESCRIBE(ayu::test::Thinger)
AYU_DESCRIBE(ayu::test::SubThing)

static tap::TestSet tests ("dirt/ayu/reflection/accessors", []{
    using namespace tap;
    SubThing thing2 {7, 8, 9, 10};

    BaseAcr<SubThing, Thing>{{}}.read(reinterpret_cast<Mu&>(thing2),
        [&](Type t, Mu* v){
            is(t, Type::For<Thing>());
            is(reinterpret_cast<Thing&>(*v).b, 8, "BaseAcr::read");
        }
    );
    BaseAcr<SubThing, Thing>{{}}.write(reinterpret_cast<Mu&>(thing2),
        [&](Type t, Mu* v){
            is(t, Type::For<Thing>());
            auto& thing = reinterpret_cast<Thing&>(*v);
            thing.a = 77;
            thing.b = 88;
        }
    );
    is(thing2.b, 88, "BaseAcr::write");
    BaseAcr<SubThing, Thinger>{{}}.write(reinterpret_cast<Mu&>(thing2),
        [&](Type t, Mu* v){
            is(t, Type::For<Thinger>());
            auto& thinger = reinterpret_cast<Thinger&>(*v);
            thinger.d = 101;
        }
    );
    is(thing2.d, 101, "BaseAcr::write (not first base)");

    auto test_addressable = [&](Str type, auto acr){
        Thing t {1, 2};
        is(
            acr.address(reinterpret_cast<Mu&>(t)),
            AnyPtr(&t.b),
            cat(type, "::address").c_str()
        );
        acr.read(reinterpret_cast<Mu&>(t),
            [&](Type t, Mu* v){
                auto ptr = AnyPtr(t, v, acr.caps);
                is(*ptr.upcast_to<const int>(), 2, cat(type, "::read").c_str());
            }
        );
        acr.write(reinterpret_cast<Mu&>(t),
            [&](Type t, Mu* v){
                auto ptr = AnyPtr(t, v, acr.caps);
                *ptr.upcast_to<int>() = 4;
            }
        );
        is(t.b, 4, cat(type, "::write").c_str());
        acr.modify(reinterpret_cast<Mu&>(t),
            [&](Type t, Mu* v){
                auto ptr = AnyPtr(t, v, acr.caps);
                *ptr.upcast_to<int>() += 5;
            }
        );
        is(t.b, 9, cat(type, "::modify").c_str());
    };
    auto test_unaddressable = [&](Str type, auto acr){
        Thing t {1, 2};
        is(
            acr.address(reinterpret_cast<Mu&>(t)).address,
            null,
            cat(type, "::address return null").c_str()
        );
        ok(!(acr.caps % AC::Address));
        acr.read(reinterpret_cast<Mu&>(t),
            [&](Type t, Mu* v){
                auto ptr = AnyPtr(t, v);
                is(*ptr.upcast_to<const int>(), 2, cat(type, "::read").c_str());
            }
        );
        acr.write(reinterpret_cast<Mu&>(t),
            [&](Type t, Mu* v){
                auto ptr = AnyPtr(t, v);
                *ptr.upcast_to<int>() = 4;
            }
        );
        is(t.b, 4, cat(type, "::write").c_str());
        acr.modify(reinterpret_cast<Mu&>(t),
            [&](Type t, Mu* v){
                auto ptr = AnyPtr(t, v);
                *ptr.upcast_to<int>() += 5;
            }
        );
        is(t.b, 9, cat(type, "::modify").c_str());
    };

    test_addressable("MemberAcr", deduce_MemberAcr(&Thing::b));
    test_addressable("RefFuncAcr", RefFuncAcr<Thing, int>{
        [](Thing& t)->int&{ return t.b; }, {}
    });
    test_unaddressable("RefFuncsAcr", RefFuncsAcr<Thing, int>{
        [](const Thing& t)->const int&{ return t.b; },
        [](Thing& t, const int& v){ t.b = v; }, {}
    });
    test_unaddressable("ValueFuncsAcr", ValueFuncsAcr<Thing, int>{
        [](const Thing& t)->int{ return t.b; },
        [](Thing& t, int v){ t.b = v; }, {}
    });
    test_unaddressable("MixedFuncsAcr", MixedFuncsAcr<Thing, int>{
        [](const Thing& t)->int{ return t.b; },
        [](Thing& t, const int& v){ t.b = v; }, {}
    });
    done_testing();
});
#endif
