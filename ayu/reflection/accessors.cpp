#include "accessors.private.h"
#include "anyref.h"
#include "describe.h"
#include "description.private.h"

namespace ayu::in {

 // noclone prevents removing unused parameter, which is necessary to match
 // signatures of other functions so they don't have to shuffle registers around
 // before jumping here.
[[gnu::noclone]] NOINLINE
void Accessor::finish_access (
    const Accessor* acr, AccessMode, Mu& to, AccessCB cb
) {
    Type t = acr->type;
    t.data |= !!(acr->flags & AcrFlags::Readonly);
    bool addressable = !(acr->flags & AcrFlags::Unaddressable);
    cb(AnyPtr(t, &to), addressable);
}

static void access_Member (
    const Accessor* acr, AccessMode mode, Mu& from, AccessCB cb
) {
    auto self = static_cast<const MemberAcr<Mu, Mu>*>(acr);
    Accessor::finish_access(acr, mode, from.*(self->mp), cb);
}

static void access_RefFunc (
    const Accessor* acr, AccessMode mode, Mu& from, AccessCB cb
) {
    auto self = static_cast<const RefFuncAcr<Mu, Mu>*>(acr);
    Accessor::finish_access(acr, mode, self->f(from), cb);
}

static void access_Variable (
    const Accessor* acr, AccessMode mode, Mu&, AccessCB cb
) {
     // Can't instantiate this with a To=Mu, because it has a To
     // embedded in it.
    auto self = static_cast<const VariableAcr<Mu, usize>*>(acr);
    Accessor::finish_access(acr, mode, *(Mu*)&self->value, cb);
}

static void access_ConstantPtr (
    const Accessor* acr, AccessMode mode, Mu&, AccessCB cb
) {
    auto self = static_cast<const ConstantPtrAcr<Mu, Mu>*>(acr);
    Accessor::finish_access(acr, mode, *const_cast<Mu*>(self->pointer), cb);
}

 // GCC's jump tables have problems with register allocation, so use our own.
 // This pathway is called a lot so it's worth optimizing it.
static constexpr AccessFunc* access_table [10] = {
    Accessor::finish_access, // Skip the noop
    access_Member,
    access_RefFunc,
    access_Variable,
    access_ConstantPtr
};

NOINLINE
void Accessor::access (AccessMode mode, Mu& from, AccessCB cb) const {
    expect(mode == AccessMode::Read ||
           mode == AccessMode::Write ||
           mode == AccessMode::Modify
    );
    expect(!(flags & AcrFlags::Readonly) || mode == AccessMode::Read);
    if (u8(form) >= u8(AF::Functive)) {
        access_func(this, mode, from, cb);
    }
    else access_table[u8(form)](this, mode, from, cb);
}

NOINLINE
void delete_Accessor (Accessor* acr) noexcept {
    switch (acr->form) {
        case AF::Variable: {
            if (!acr->ref_count) break; // Don't try to destruct constexpr object
             // Can't use Mu because it doesn't have a size.  We don't *need* a
             // size, but C++ does in order to do this operation.  So I guess
             // use usize instead?  Hope the alignment works out!
            auto* self = static_cast<VariableAcr<Mu, usize>*>(acr);
            self->type.destroy((Mu*)&self->value);
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
         // all be statically generated, so if two ACRs refer to the same member
         // of a type, they should have the same address.
         // TODO: We now have more type information, so we can compare more
         // types of accessors.
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

void AnyRefFuncAcr1::_access (
    const Accessor* acr, AccessMode mode, Mu& from, AccessCB cb
) {
    auto self = static_cast<const AnyRefFuncAcr<Mu>*>(acr);
     // Just pass on the call
    self->f(from).access(mode, cb);
}

void AnyPtrFuncAcr1::_access (
    const Accessor* acr, AccessMode, Mu& from, AccessCB cb
) {
    auto self = static_cast<const AnyPtrFuncAcr<Mu>*>(acr);
    auto ptr = self->f(from);
    ptr.type.data |= !!(self->flags & AcrFlags::Readonly);
    cb(ptr, !(self->flags & AcrFlags::Unaddressable));
}

static
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

ChainAcr::ChainAcr (const Accessor* outer, const Accessor* inner) noexcept :
    Accessor(AF::Chain, &_access, chain_acr_flags(outer->flags, inner->flags)),
    outer(outer), inner(inner)
{ outer->inc(); inner->inc(); }

void ChainAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, AccessCB cb
) {
    struct Frame {
        const ChainAcr* self;
        AccessCB cb;
        AccessMode mode;
        bool o_addr;
    };
    Frame frame {static_cast<const ChainAcr*>(acr), cb, mode, false};
     // Have to use modify instead of write for the first mode, or other
     // parts of the item will get clobbered.  Hope this isn't necessary
     // very often.
    auto outer_mode = write_to_modify(mode);
    return frame.self->outer->access(outer_mode, v,
        AccessCB(frame, [](Frame& frame, AnyPtr w, bool o_addr){
            expect(!w.readonly() || frame.mode == AccessMode::Read);
            frame.o_addr = o_addr;
            frame.self->inner->access(frame.mode, *w.address,
                AccessCB(frame, [](Frame& frame, AnyPtr x, bool i_addr){
                     // We need to wrap the cb twice, so that we can return the
                     // correct addressable bool, for a total of five nested
                     // indirect calls.  Fortunately, many of them can be tail
                     // calls, and there are no branches in these functions.
                    bool addr = frame.o_addr & i_addr
                              & !(frame.self->flags & AcrFlags::Unaddressable);
                    frame.cb(x, addr);
                })
            );
        })
    );
}

ChainAcr::~ChainAcr () { inner->dec(); outer->dec(); }

void ChainAttrFuncAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, AccessCB cb
) {
    struct Frame {
        const ChainAttrFuncAcr* self;
        AccessCB cb;
        AccessMode mode;
        bool o_addr;
    };
    Frame frame {static_cast<const ChainAttrFuncAcr*>(acr), cb, mode, false};
    auto outer_mode = write_to_modify(mode);
    frame.self->outer->access(outer_mode, v,
        AccessCB(frame, [](Frame& frame, AnyPtr w, bool o_addr){
            expect(!w.readonly() || frame.mode == AccessMode::Read);
            frame.o_addr = o_addr;
            frame.self->f(*w.address, frame.self->key).access(frame.mode,
                AccessCB(frame, [](Frame& frame, AnyPtr x, bool i_addr){
                    bool addr = frame.o_addr & i_addr
                              & !(frame.self->flags & AcrFlags::Unaddressable);
                    frame.cb(x, addr);
                })
            );
        })
    );
}

void ChainElemFuncAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, AccessCB cb
) {
    struct Frame {
        const ChainElemFuncAcr* self;
        AccessCB cb;
        AccessMode mode;
        bool o_addr;
    };
    Frame frame {static_cast<const ChainElemFuncAcr*>(acr), cb, mode, false};
    auto outer_mode = write_to_modify(mode);
    frame.self->outer->access(outer_mode, v,
        AccessCB(frame, [](Frame& frame, AnyPtr w, bool o_addr){
            expect(!w.readonly() || frame.mode == AccessMode::Read);
            frame.o_addr = o_addr;
            frame.self->f(*w.address, frame.self->index).access(frame.mode,
                AccessCB(frame, [](Frame& frame, AnyPtr x, bool i_addr){
                    bool addr = frame.o_addr & i_addr
                              & !(frame.self->flags & AcrFlags::Unaddressable);
                    frame.cb(x, addr);
                })
            );
        })
    );
}

void ChainDataFuncAcr::_access (
    const Accessor* acr, AccessMode mode, Mu& v, AccessCB cb
) {
    struct Frame {
        const ChainDataFuncAcr* self;
        AccessCB cb;
#ifndef NDEBUG
        AccessMode mode;
#endif
    };
#ifndef NDEBUG
    Frame frame {static_cast<const ChainDataFuncAcr*>(acr), cb, mode};
#else
    Frame frame {static_cast<const ChainDataFuncAcr*>(acr), cb};
#endif
    auto outer_mode = write_to_modify(mode);
    frame.self->outer->access(outer_mode, v,
        AccessCB(frame, [](Frame& frame, AnyPtr w, bool o_addr){
             // We should already have done bounds checking.  Unfortunately we
             // can't reverify it in debug mode because we've lost the info
             // necessary to get the length.
#ifndef NDEBUG
            expect(!w.readonly() || frame.mode == AccessMode::Read);
#endif
            AnyPtr x = frame.self->f(*w.address);
            x.address = (Mu*)(
                (char*)x.address + frame.self->index * x.type.cpp_size()
            );
            bool addr = o_addr
                      & !(frame.self->flags & AcrFlags::Unaddressable);
            frame.cb(x, addr);
        })
    );
}

} using namespace ayu::in;
using namespace ayu;

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

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
        [&](AnyPtr thing, bool){
            is(thing.type, Type::For<Thing>());
            is(reinterpret_cast<Thing&>(*thing.address).b, 8, "BaseAcr::read");
        }
    );
    BaseAcr<SubThing, Thing>{{}}.write(reinterpret_cast<Mu&>(thing2),
        [&](AnyPtr thing, bool){
            is(thing.type, Type::For<Thing>());
            auto& th = reinterpret_cast<Thing&>(*thing.address);
            th.a = 77;
            th.b = 88;
        }
    );
    is(thing2.b, 88, "BaseAcr::write");
    BaseAcr<SubThing, Thinger>{{}}.write(reinterpret_cast<Mu&>(thing2),
        [&](AnyPtr thinger, bool){
            is(thinger.type, Type::For<Thinger>());
            auto& thr = reinterpret_cast<Thinger&>(*thinger.address);
            thr.d = 101;
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
            [&](AnyPtr v, bool){
                is(*v.upcast_to<const int>(), 2, cat(type, "::read").c_str());
            }
        );
        acr.write(reinterpret_cast<Mu&>(t),
            [&](AnyPtr v, bool){
                *v.upcast_to<int>() = 4;
            }
        );
        is(t.b, 4, cat(type, "::write").c_str());
        acr.modify(reinterpret_cast<Mu&>(t),
            [&](AnyPtr v, bool){
                *v.upcast_to<int>() += 5;
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
        acr.read(reinterpret_cast<Mu&>(t),
            [&](AnyPtr v, bool){
                is(*v.upcast_to<const int>(), 2, cat(type, "::read").c_str());
            }
        );
        acr.write(reinterpret_cast<Mu&>(t),
            [&](AnyPtr v, bool){
                *v.upcast_to<int>() = 4;
            }
        );
        is(t.b, 4, cat(type, "::write").c_str());
        acr.modify(reinterpret_cast<Mu&>(t),
            [&](AnyPtr v, bool){
                *v.upcast_to<int>() += 5;
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
