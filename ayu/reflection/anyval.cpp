#include "anyval.h"

#include "describe.h"
#include "reference.h"

using namespace ayu;
using namespace ayu::in;

 // We need to use values_custom
AYU_DESCRIBE(ayu::AnyVal,
    values_custom(
        [](const AnyVal& a, const AnyVal& b) -> bool {
            expect(!b);
            return !a;
        },
        [](AnyVal& a, const AnyVal& b) {
            expect(!b);
            a = AnyVal();
        },
        value(Tree::array(), AnyVal())
    ),
    elems(
        elem(value_funcs<Type>(
            [](const AnyVal& v){ return v.type; },
            [](AnyVal& v, Type t){
                v = AnyVal(t);
            }
        )),
        elem(reference_func([](AnyVal& v){ return Reference(v.ptr()); }))
    )
)

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"
#include "../data/parse.h"
#include "../traversal/from-tree.h"
#include "../traversal/to-tree.h"

namespace ayu::test {
    struct AnyValTest {
        int a;
        int b;
    };

    struct Test2 {
        int a;
    };
    bool operator == (const Test2& a, const Test2& b) {
        return a.a == b.a;
    }

    struct NoConstructor {
        NoConstructor () = delete;
    };
    struct CustomConstructor {
        CustomConstructor () = delete;
        ~CustomConstructor () = delete;
    };

    struct NoCopy {
        NoCopy () { }
        NoCopy (const NoCopy&) = delete;
    };

    struct NoDestructor {
        ~NoDestructor () = delete;
    };
    struct alignas(256) WeirdAlign {
        WeirdAlign () {
            if (reinterpret_cast<usize>(this) & (256-1)) {
                throw std::runtime_error("Aligned allocation didn't work");
            }
        }
    };
} using namespace ayu::test;

 // The things here should work without any descriptions
AYU_DESCRIBE(ayu::test::AnyValTest)
AYU_DESCRIBE(ayu::test::Test2)
AYU_DESCRIBE(ayu::test::NoConstructor)
AYU_DESCRIBE(ayu::test::NoCopy)
AYU_DESCRIBE(ayu::test::NoDestructor)
AYU_DESCRIBE(ayu::test::WeirdAlign)

AYU_DESCRIBE(ayu::test::CustomConstructor,
    default_construct([](void*){ }),
    destroy([](CustomConstructor*){ })
)

static tap::TestSet tests ("dirt/ayu/reflection/dynamic", []{
    using namespace tap;
    AnyVal d;
    ok(!d, "Default AnyVal is empty");
    d = AnyVal::make<bool>(true);
    ok(d.as<bool>(), "Can make AnyVal bool");
    d = AnyVal::make<bool>(false);
    ok(!d.as<bool>(), "Can make AnyVal false bool");
    ok(!!d, "AnyVal false bool is not empty");
    d = AnyVal::make<AnyValTest>(4, 5);
    is(d.as<AnyValTest>().b, 5, "Can make AnyVal with struct type");
    throws_code<e_TypeCantCast>([&]{ d.as<bool>(); }, "TypeCantCast");
    throws_code<e_TypeCantDefaultConstruct>([&]{
        AnyVal(Type::CppType<NoConstructor>());
    }, "TypeCantDefaultConstruct");
    throws_code<e_TypeCantDestroy>([&]{
        d = AnyVal(Type::CppType<NoDestructor>());
    }, "Cannot construct type without destructor");

    doesnt_throw([&]{
        d = AnyVal(Type::CppType<CustomConstructor>());
    }, "Can construct type with externally-supplied constructor/destructor");

    d = AnyVal::make<int32>(4);
    is(item_to_tree(&d), tree_from_string("[int32 4]"), "AnyVal to_tree works");
    doesnt_throw([&]{
        item_from_string(&d, "[double 55]");
    });
    is(d.type, Type::CppType<double>(), "AnyVal from_tree gives correct type");
    is(d.as<double>(), double(55), "AnyVal from_tree gives correct value");
    doesnt_throw([&]{
        item_from_string(&d, "[]");
    });
    ok(!d, "AnyVal from_tree with [] makes empty AnyVal");
    doesnt_throw([&]{
        d = AnyVal::make<WeirdAlign>();
    }, "Can allocate object with non-standard alignment");
    is(usize(d.data) & 255, 0u, "Weird alignment data has correct alignment");

    done_testing();
});
#endif
