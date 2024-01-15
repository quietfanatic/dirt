#include "dynamic.h"

#include "describe.h"
#include "reference.h"

using namespace ayu;
using namespace ayu::in;

 // We need to use values_custom
AYU_DESCRIBE(ayu::Dynamic,
    values_custom(
        [](const Dynamic& a, const Dynamic& b) -> bool {
            expect(!b.has_value());
            return !a.has_value();
        },
        [](Dynamic& a, const Dynamic& b) {
            expect(!b.has_value());
            a = Dynamic();
        },
        value(Tree::array(), Dynamic())
    ),
    elems(
        elem(value_funcs<Type>(
            [](const Dynamic& v){ return v.type; },
            [](Dynamic& v, Type t){
                v = Dynamic(t);
            }
        )),
        elem(reference_func([](Dynamic& v){ return Reference(v.ptr()); }))
    )
)

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"
#include "../data/parse.h"
#include "../traversal/from-tree.h"
#include "../traversal/to-tree.h"

namespace ayu::test {
    struct DynamicTest {
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
AYU_DESCRIBE(ayu::test::DynamicTest)
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
    Dynamic d;
    ok(!d.has_value(), "Default Dynamic::has_value is false");
    d = Dynamic::make<bool>(true);
    ok(d.as<bool>(), "Can make Dynamic bool");
    d = Dynamic::make<bool>(false);
    ok(!d.as<bool>(), "Can make Dynamic false bool");
    ok(d.has_value(), "Dynamic false bool has_value");
    d = Dynamic::make<DynamicTest>(4, 5);
    is(d.as<DynamicTest>().b, 5, "Can make Dynamic with struct type");
    throws_code<e_TypeCantCast>([&]{ d.as<bool>(); }, "TypeCantCast");
    throws_code<e_TypeCantDefaultConstruct>([&]{
        Dynamic(Type::CppType<NoConstructor>());
    }, "TypeCantDefaultConstruct");
    throws_code<e_TypeCantDestroy>([&]{
        d = Dynamic(Type::CppType<NoDestructor>());
    }, "Cannot construct type without destructor");

    doesnt_throw([&]{
        d = Dynamic(Type::CppType<CustomConstructor>());
    }, "Can construct type with externally-supplied constructor/destructor");

    d = Dynamic::make<int32>(4);
    is(item_to_tree(&d), tree_from_string("[int32 4]"), "Dynamic to_tree works");
    doesnt_throw([&]{
        item_from_string(&d, "[double 55]");
    });
    is(d.type, Type::CppType<double>(), "Dynamic from_tree gives correct type");
    is(d.as<double>(), double(55), "Dynamic from_tree gives correct value");
    doesnt_throw([&]{
        item_from_string(&d, "[]");
    });
    ok(!d.has_value(), "Dynamic from_tree with [] makes unhas_value Dynamic");
    doesnt_throw([&]{
        d = Dynamic::make<WeirdAlign>();
    }, "Can allocate object with non-standard alignment");
    is(usize(d.data) & 255, 0u, "Weird alignment data has correct alignment");

    done_testing();
});
#endif
