// Provides ayu descriptions for builtin scalar types.  For template types like
// std::vector, include the .h file.

#include "../../iri/iri.h"
#include "../../uni/utf.h"
#include "describe-standard.h"

namespace ayu::in {
    [[gnu::noclone]] NOINLINE
    AnyString make_optional_name (Type t) noexcept {
        return cat(t.name(), '?');
    }
    [[gnu::noclone]] NOINLINE
    AnyString make_pointer_name (Type t, int flags) noexcept {
        UniqueString r = t.name();
        if (flags & 1) encat(r, " const");
        if (flags & 2) encat(r, " volatile");
        return cat(move(r), '*');
    }
    [[gnu::noclone]] NOINLINE
    AnyString make_template_name_1 (StaticString prefix, Type t) noexcept {
        return cat(prefix, t.name(), '>');
    }
    [[gnu::noclone]] NOINLINE
    AnyString make_variadic_name (
        StaticString prefix,
        const Type* types,
        u32 len
    ) noexcept {
        expect(len >= 1);
        return cat(
            prefix, Caterator(", ", len, [types](u32 i){
                 // This will call get_type_name twice for each desc, but that
                 // should still be faster and smaller than caching all the
                 // names in a variable-length array.
                return expect(types[i].name());
            }), '>'
        );
    }
} // ayu::in

using namespace ayu;

AYU_DESCRIBE(std::string,
    to_tree([](const std::string& v){ return Tree(Str(v)); }),
    from_tree([](std::string& v, const Tree& t){ v = std::string(Str(t)); })
)
AYU_DESCRIBE(std::u16string,
    to_tree([](const std::u16string& v){
        return Tree(uni::from_utf16(v));
    }),
     // Inefficient but I don't really care about std::u16string, I'm just using
     // it for testing
    from_tree([](std::u16string& v, const Tree& t){
        v = uni::to_utf16(Str(t));
    })
)

AYU_DESCRIBE(std::string_view,
    to_tree([](const std::string_view& v){
        return Tree(Str(v));
    })
)
AYU_DESCRIBE(std::u16string_view,
    to_tree([](const std::u16string_view& v){
        return Tree(uni::from_utf16(v));
    })
)

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"
#include "../traversal/from-tree.h"
#include "../traversal/to-tree.h"

static tap::TestSet tests ("dirt/ayu/reflection/describe-standard", []{
    using namespace tap;
     // Test wstrings
    std::string s8 = "\"あいうえお\"";
    std::u16string s16 = u"あいうえお";
    is(item_to_string(&s16), s8, "Can serialize wstring");
    std::u16string s16_got;
    doesnt_throw([&]{
        item_from_string(&s16_got, s8);
    });
    is(s16_got, s16, "Can deserialize wstring");
     // Test tuples
    std::tuple<i32, std::string, std::vector<i32>> data;
    std::tuple<i32, std::string, std::vector<i32>> expected_data
        = {45, "asdf", {3, 4, 5}};
    Str s = "[45 asdf [3 4 5]]";
    doesnt_throw([&]{
        return item_from_string(&data, s);
    }, "item_from_string on tuple");
    is(data, expected_data, "gives correct result");
    std::string got_s;
    doesnt_throw([&]{
        got_s = item_to_string(&expected_data);
    }, "item_to_string on tuple");
    is(got_s, s, "gives correct result");
     // Test uni arrays
    uni::AnyArray<uni::AnyString> strings {
        "asdf", "fdsa", "foo", "bar"
    };
    StaticString strings_s = "[asdf fdsa foo bar]";
    is(item_to_string(&strings), strings_s, "uni arrays and strings");
    done_testing();
});

#endif
