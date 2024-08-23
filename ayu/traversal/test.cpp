#include "compound.h"
#include "from-tree.h"
#include "to-tree.h"

using namespace ayu;
using namespace ayu::in;

#ifndef TAP_DISABLE_TESTS
#include <unordered_map>
#include "../../tap/tap.h"
#include "../reflection/describe.h"

 // Putting these in a test namespace so their described names don't conflict
namespace ayu::test {
    struct ToTreeTest {
        int value;
    };

    enum ValuesTest {
        VTA,
        VTNULL,
        VTZERO,
        VTNAN
    };

    struct MemberTest {
        int a;
        int b;
         // Testing absence of copy constructor
        MemberTest (const MemberTest&) = delete;
         // C++20 doesn't let you aggregate-initialize classes with ANY
         // declared or deleted constructors any more.
        MemberTest (int a, int b) : a(a), b(b) { }
    };

    struct PrivateMemberTest {
        PrivateMemberTest (int s) : stuff(s) { }
      private:
        int stuff;
        AYU_FRIEND_DESCRIBE(PrivateMemberTest)
    };

    struct BaseTest : MemberTest {
        int c;
    };

    struct IncludeTest : BaseTest {
        int d;
    };

    struct ElemTest {
        float x;
        float y;
        float z;
        void foo ();
    };

    struct ElemsTest {
        std::vector<int> xs;
    };

     // Test usage of keys() with type AnyArray<AnyString>
    struct AttrsTest2 {
        std::unordered_map<AnyString, int> xs;
    };

    struct DelegateTest {
        ElemTest et;
    };
    struct SwizzleTest {
        bool swizzled = false;
    };
    struct InitTest {
        int value;
        int value_after_init = 0;
    };
    struct LateInitTest {
        int* place;
        int value_after_init = 0;
    };
    struct NestedInitTest {
        LateInitTest lit;
        InitTest it;
        int it_val = -1;
    };
    enum ScalarElemTest : uint8 {
    };
    struct InternalRefTest {
        int a;
        int b;
        int* p;
    };
    struct ChainRefTest {
        Reference ref;
         // Make this non-addressable to test chaining an elem func onto a
         // non-addressable reference.
        std::vector<int> target;
    };
} using namespace ayu::test;

AYU_DESCRIBE(ayu::test::ToTreeTest,
    to_tree([](const ToTreeTest& x){ return Tree(x.value); }),
    from_tree([](ToTreeTest& x, const Tree& t){ x.value = int(t); })
)
const ValuesTest vtnan = VTNAN;
AYU_DESCRIBE(ayu::test::ValuesTest,
    values(
        value("vta", VTA),
        value(null, VTNULL),
        value(int(0), VTZERO),
        value_ptr(nan, &vtnan)
    )
)
AYU_DESCRIBE(ayu::test::MemberTest,
    attrs(
        attr("a", member(&MemberTest::a)),
        attr("b", &MemberTest::b)  // Implicit member()
    )
)
AYU_DESCRIBE(ayu::test::PrivateMemberTest,
    attrs(
        attr("stuff", &PrivateMemberTest::stuff)
    )
)

AYU_DESCRIBE(ayu::test::BaseTest,
    attrs(
        attr("MemberTest", base<MemberTest>()),
        attr("c", member(&BaseTest::c))
    )
)
AYU_DESCRIBE(ayu::test::IncludeTest,
    attrs(
        attr("BaseTest", base<BaseTest>(), include),
        attr("d", &IncludeTest::d)
    )
)
AYU_DESCRIBE(ayu::test::ElemTest,
    elems(
        elem(member(&ElemTest::x)),
        elem(&ElemTest::y),
        elem(member(&ElemTest::z))
    )
)
AYU_DESCRIBE(ayu::test::ElemsTest,
    length(value_funcs<usize>(
        [](const ElemsTest& v){
            return v.xs.size();
        },
        [](ElemsTest& v, usize l){
            v.xs.resize(l);
        }
    )),
    computed_elems([](ElemsTest& v, usize i){
        return Reference(&v.xs.at(i));
    })
)
AYU_DESCRIBE(ayu::test::AttrsTest2,
    keys(mixed_funcs<AnyArray<AnyString>>(
        [](const AttrsTest2& v){
            AnyArray<AnyString> r;
            for (auto& p : v.xs) {
                r.emplace_back(p.first);
            }
            return r;
        },
        [](AttrsTest2& v, const AnyArray<AnyString>& ks){
            v.xs.clear();
            for (auto& k : ks) {
                v.xs.emplace(k, 0);
            }
        }
    )),
    computed_attrs([](AttrsTest2& v, const AnyString& k){
        return Reference(&v.xs.at(k));
    })
)
AYU_DESCRIBE(ayu::test::DelegateTest,
    delegate(member(&DelegateTest::et))
)
AYU_DESCRIBE(ayu::test::SwizzleTest,
    swizzle([](SwizzleTest& v, const Tree&){
        v.swizzled = true;
    })
)
AYU_DESCRIBE(ayu::test::InitTest,
    delegate(member(&InitTest::value)),
    init([](InitTest& v){
        v.value_after_init = v.value + 1;
    })
)
AYU_DESCRIBE(ayu::test::LateInitTest,
    attrs(),
    init([](LateInitTest& v){
        v.value_after_init = *v.place + 1;
    }, -10)
)
AYU_DESCRIBE(ayu::test::NestedInitTest,
    attrs(
        attr("lit", &NestedInitTest::lit),
        attr("it", &NestedInitTest::it)
    ),
    init([](NestedInitTest& v){
        v.it_val = v.it.value_after_init;
    })
)
AYU_DESCRIBE(ayu::test::ScalarElemTest,
    elems(
        elem(value_funcs<uint8>(
            [](const ScalarElemTest& v) -> uint8 {
                return uint8(v) >> 4;
            },
            [](ScalarElemTest& v, uint8 m){
                v = ScalarElemTest((uint8(v) & 0xf) | (m << 4));
            }
        )),
        elem(value_funcs<uint8>(
            [](const ScalarElemTest& v) -> uint8 {
                return uint8(v) & 0xf;
            },
            [](ScalarElemTest& v, uint8 m){
                v = ScalarElemTest((uint8(v) & 0xf0) | (m & 0xf));
            }
        ))
    )
)
AYU_DESCRIBE(ayu::test::InternalRefTest,
    attrs(
        attr("a", &InternalRefTest::a),
        attr("b", &InternalRefTest::b),
        attr("p", &InternalRefTest::p)
    )
)
AYU_DESCRIBE(ayu::test::ChainRefTest,
    attrs(
        attr("ref", &ChainRefTest::ref),
        attr("target", member(&ChainRefTest::target, unaddressable))
    )
)

static tap::TestSet tests ("dirt/ayu/traversal", []{
    using namespace tap;
    ok(get_description_for_name("ayu::test::MemberTest"), "Description was registered");

    auto try_to_tree = [](Reference item, Str tree, Str name){
        try_is(
            [&item]{ return item_to_tree(item); },
            tree_from_string(tree),
            name
        );
    };

    auto ttt = ToTreeTest{5};
    try_to_tree(&ttt, "5", "item_to_tree works with to_tree descriptor");

    ValuesTest vtt = VTA;
    try_to_tree(&vtt, "\"vta\"", "item_to_tree works with string value");
    vtt = VTNULL;
    try_to_tree(&vtt, "null", "item_to_tree works with null value");
    vtt = VTZERO;
    try_to_tree(&vtt, "0", "item_to_tree works with int value");
    vtt = VTNAN;
    try_to_tree(&vtt, "+nan", "item_to_tree works with double value");
    vtt = ValuesTest(999);
    doesnt_throw([&]{ item_from_string(&vtt, "\"vta\""); });
    is(vtt, VTA, "item_from_tree works with string value");
    doesnt_throw([&]{ item_from_string(&vtt, "null"); });
    is(vtt, VTNULL, "item_from_tree works with null value");
    doesnt_throw([&]{ item_from_string(&vtt, "0"); });
    is(vtt, VTZERO, "item_from_tree works with int value");
    doesnt_throw([&]{ item_from_string(&vtt, "+nan"); });
    is(vtt, VTNAN, "item_from_tree works with double value");

    auto mt = MemberTest(3, 4);
    Tree mtt = item_to_tree(&mt);
    is(mtt, tree_from_string("{a:3 b:4}"), "item_to_tree works with attrs descriptor");

    auto pmt = PrivateMemberTest(4);
    Tree pmtt = item_to_tree(&pmt);
    is(pmtt, tree_from_string("{stuff:4}"), "AYU_FRIEND_DESCRIBE works");

    item_from_string(&mt, "{a:87 b:11}");
    is(mt.a, 87, "item_from_tree works with attrs descriptor (a)");
    is(mt.b, 11, "item_from_tree works with attrs descriptor (b)");
    item_from_string(&mt, "{b:92 a:47}");
    is(mt.a, 47, "item_from_tree works with attrs out of order (a)");
    is(mt.b, 92, "item_from_tree works with attrs out of order (b)");
    throws_code<e_AttrMissing>(
        [&]{ item_from_string(&mt, "{a:16}"); },
        "item_from_tree throws on missing attr with attrs descriptor"
    );
    throws_code<e_TreeWrongForm>(
        [&]{ item_from_string(&mt, "{a:41 b:foo}"); },
        "item_from_tree throws when attr has wrong form"
    );
    throws_code<e_TreeCantRepresent>(
        [&]{ item_from_string(&mt, "{a:41 b:4.3}"); },
        "item_from_tree throws when int attr isn't integer"
    );
    throws_code<e_FromTreeFormRejected>(
        [&]{ item_from_string(&mt, "[54 43]"); },
        "item_from_tree throws when trying to make attrs object from array"
    );
    throws_code<e_AttrRejected>(
        [&]{ item_from_string(&mt, "{a:0 b:1 c:60}"); },
        "item_from_tree throws on extra attr"
    );

    auto bt = BaseTest{{-1, -2}, -3};
    Tree btt = item_to_tree(&bt);
    is(btt, tree_from_string("{MemberTest:{a:-1,b:-2} c:-3}"), "item_to_tree with base attr");
    Tree from_tree_bt1 = tree_from_string("{c:-4,MemberTest:{a:-5,b:-6}}");
    item_from_tree(&bt, from_tree_bt1);
    is(bt.b, -6, "item_from_tree with base attr");
    throws_code<e_AttrMissing>(
        [&]{ item_from_string(&bt, "{a:-7,b:-8,c:-9}"); },
        "item_from_tree with base attr throws when collapsed but include is not specified"
    );

    auto it = IncludeTest{{{99, 88}, 77}, 66};
    Tree itt = item_to_tree(&it);
    is(itt, tree_from_string("{MemberTest:{a:99,b:88} c:77 d:66}"), "Include works with item_to_tree");
    Tree from_tree_it1 = tree_from_string("{d:55 c:44 MemberTest:{a:33 b:22}}");
    item_from_tree(&it, from_tree_it1);
    is(it.a, 33, "Include works with item_from_tree");
    Tree from_tree_it2 = tree_from_string("{d:51 BaseTest:{c:41 MemberTest:{b:31 a:21}}}");
    item_from_tree(&it, from_tree_it2);
    is(it.b, 31, "Include works when not collapsed");

    auto et = ElemTest{0.5, 1.5, 2.5};
    Tree ett = item_to_tree(&et);
    is(ett, tree_from_string("[0.5 1.5 2.5]"), "item_to_tree with elems descriptor");
    Tree from_tree_et1 = tree_from_string("[3.5 4.5 5.5]");
    item_from_tree(&et, from_tree_et1);
    is(et.y, 4.5, "item_from_tree with elems descriptor");
    throws_code<e_LengthRejected>(
        [&]{ item_from_string(&et, "[6.5 7.5]"); },
        "item_from_tree throws on too short array with elems descriptor"
    );
    throws_code<e_LengthRejected>(
        [&]{ item_from_string(&et, "[6.5 7.5 8.5 9.5]"); },
        "item_from_tree throws on too long array with elems descriptor");
    throws_code<e_FromTreeFormRejected>([&]{
        item_from_string(&et, "{x:1.1 y:2.2}");
    }, "item_from_tree throws when trying to make elems thing from object");

    auto est = ElemsTest{{1, 3, 6, 10, 15, 21}};
    is(item_get_length(&est), 6u, "item_get_length");
    int answer = 0;
    doesnt_throw([&]{
        item_elem(&est, 5).read_as<int>([&](const int& v){ answer = v; });
    }, "item_elem and Reference::read_as");
    is(answer, 21, "item_elem gives correct answer");
    throws_code<e_External>(
        [&]{ item_elem(&est, 6); },
        "item_elem can throw on out of bounds index (from user-defined function)"
    );
    item_set_length(&est, 5);
    is(est.xs.size(), 5u, "item_set_length shrink");
    throws_code<e_External>([&]{
        item_elem(&est, 5);
    }, "item_elem reflects new length");
    item_set_length(&est, 9);
    is(est.xs.size(), 9u, "item_set_length grow");
    doesnt_throw([&]{
        item_elem(&est, 8).write_as<int>([](int& v){ v = 99; });
    }, "item_elem and Reference::write_as");
    is(est.xs.at(8), 99, "writing to elem works");
    try_to_tree(&est, "[1 3 6 10 15 0 0 0 99]", "item_to_tree with length and computed_elems");
    doesnt_throw([&]{
        item_from_string(&est, "[5 2 0 4]");
    }, "item_from_tree with length and computed_elems doesn't throw");
    is(est.xs.at(3), 4, "item_from_tree works with computed_elems");

    auto ast2 = AttrsTest2{{{"a", 11}, {"b", 22}}};
    auto keys = item_get_keys(&ast2);
    is(keys.size(), 2u, "item_get_keys (size)");
    ok((keys[0] == "a" && keys[1] == "b") || (keys[0] == "b" && keys[1] == "a"),
        "item_get_keys (contents)"
    );
    answer = 0;
    doesnt_throw([&]{
        item_attr(&ast2, "b").read_as<int>([&](const int& v){ answer = v; });
    }, "item_attr and Reference::read_as");
    is(answer, 22, "item_attr gives correct answer");
    throws_code<e_External>([&]{
        item_attr(&ast2, "c");
    }, "item_attr can throw on missing key (from user-defined function)");
    auto ks = std::vector<AnyString>{"c", "d"};
    item_set_keys(&ast2, Slice<AnyString>(ks));
    is(ast2.xs.find("a"), ast2.xs.end(), "item_set_keys removed key");
    is(ast2.xs.at("c"), 0, "item_set_keys added key");
    doesnt_throw([&]{
        item_attr(&ast2, "d").write_as<int>([](int& v){ v = 999; });
    }, "item_attr and Reference::write_as");
    is(ast2.xs.at("d"), 999, "writing to attr works");
    try_to_tree(&ast2, "{c:0,d:999}", "item_to_tree with keys and computed_attrs");
    doesnt_throw([&]{
        item_from_string(&ast2, "{e:88,f:34}");
    }, "item_from_tree with keys and computed_attrs doesn't throw");
    is(ast2.xs.at("f"), 34, "item_from_tree works with computed_attrs");

    auto dt = DelegateTest{{4, 5, 6}};
    try_to_tree(&dt, "[4 5 6]", "item_to_tree with delegate");
    doesnt_throw([&]{
        item_from_string(&dt, "[7 8 9]");
    });
    is(dt.et.y, 8, "item_from_tree with delegate");
    is(item_elem(&dt, 2).address_as<float>(), &dt.et.z, "item_elem works with delegate");

    std::vector<ToTreeTest> tttv {{444}, {333}};
    try_to_tree(&tttv, "[444 333]", "template describe on std::vector works");
    doesnt_throw([&]{
        item_from_string(&tttv, "[222 111 666 555]");
    });
    is(tttv[3].value, 555, "from_tree works with template describe on std::vector");

    std::vector<SwizzleTest> stv;
    doesnt_throw([&]{
        item_from_string(&stv, "[{}{}{}{}{}{}]");
    });
    ok(stv.at(4).swizzled, "Basic swizzle works");

    InitTest initt {4};
    doesnt_throw([&]{
        item_from_string(&initt, "6");
    });
    is(initt.value_after_init, 7, "Basic init works");
    NestedInitTest nit {{null}, {3}};
    nit.lit.place = &nit.it_val;
    doesnt_throw([&]{
        item_from_string(&nit, "{lit:{} it:55}");
    });
    is(nit.it_val, 56, "Children get init() before parent");
    is(nit.lit.value_after_init, 57, "init() with lower priority gets called after");

    ScalarElemTest set = ScalarElemTest(0xab);
    try_to_tree(&set, "[0xa 0xb]", "Can use elems() on scalar type (to_tree)");
    doesnt_throw([&]{
        item_from_string(&set, "[0xc 0xd]");
    });
    is(set, ScalarElemTest(0xcd), "Can use elems() on scalar type (from_tree)");

    InternalRefTest irt = {3, 4, null};
    irt.p = &irt.a;
    try_to_tree(&irt, "{a:3 b:4 p:#/a}", "Can serialize item with internal refs");
    doesnt_throw([&]{
        item_from_string(&irt, "{a:5 b:6 p:#/b}");
    });
    is(irt.p, &irt.b, "Can deserialize item with internal refs");

    ChainRefTest crt = {null, {5, 4, 3}};
    crt.ref = Reference(&crt)["target"][1];
    try_is([&]{ return crt.ref.get_as<int>(); }, 4, "Can read from complex unaddressable ref");
    doesnt_throw([&]{ crt.ref.set_as<int>(6); }, "Can write to complex unaddressable ref");
    is(crt.target[1], 6);
    try_to_tree(&crt, "{ref:#/target+1 target:[5 6 3]}", "Can serialize item with complex unaddressable ref");
    doesnt_throw([&]{
        item_from_string(&crt, "{ref:#/target+2 target:[0 2 9 6]}");
    });
    is(crt.ref, Reference(&crt)["target"][2], "Can deserialize item with complex unaddressable ref");
    try_is([&]{ return crt.ref.get_as<int>(); }, 9, "Can read from complex unaddressable ref after deserializing");
    doesnt_throw([&]{ crt.ref.set_as<int>(7); }, "Can write to complex unaddressable ref after deserializing");
    is(crt.target[2], 7);

    done_testing();
});
#endif
