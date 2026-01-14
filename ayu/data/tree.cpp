#include "tree.h"

#include "../reflection/describe.h"
#include "../traversal/to-tree.h"

namespace ayu {
namespace in {

void check_uniqueness (u32 s, const TreePair* p) {
    expect(s >= 2);
    auto e = p+s;
    auto a = p+1;
    do {
        auto b = p;
        do {
            if (a->first == b->first) {
                raise(e_TreeObjectKeyDuplicate, a->first);
            }
        } while (++b < a);
    } while (++a < e);
}

NOINLINE
void delete_Tree_data (Tree& t) noexcept {
     // Manually delete all the elements.  We can't call UniqueArray<*>'s
     // destructor because we've already run the reference count down to 0, and
     // it debug-asserts that the reference count is 1.
    expect(t.owned);
    switch (t.form) {
        case Form::String: {
            SharableBuffer<const char>::deallocate(t.data.as_char_ptr);
            break;
        }
        case Form::Array: {
            for (auto& e : Slice<Tree>(t)) {
                e.~Tree();
            }
            SharableBuffer<const Tree>::deallocate(t.data.as_array_ptr);
            break;
        }
        case Form::Object: {
            for (auto& p : Slice<TreePair>(t)) {
                p.~TreePair();
            }
            SharableBuffer<const TreePair>::deallocate(t.data.as_object_ptr);
            break;
        }
        case Form::Error: {
            t.data.as_error_ptr->~exception_ptr();
            SharableBuffer<const std::exception_ptr>::deallocate(t.data.as_error_ptr);
            break;
        }
        default: never();
    }
}

void raise_TreeWrongForm (const Tree& t, Form form) {
    if (t.form == Form::Error) std::rethrow_exception(std::exception_ptr(t));
    else raise(e_TreeWrongForm, cat(
        "Expected ", show(&form), " but got ", show(&t.form)
    ));
}

void raise_TreeCantRepresent (StaticString type_name, const Tree& t) {
    raise(e_TreeCantRepresent, cat(
        "Can't represent type ", type_name, " with value ", tree_to_string(t)
    ));
}

static bool tree_eq_false (const Tree&, const Tree&) noexcept { return false; }
static bool tree_eq_true (const Tree&, const Tree&) noexcept { return true; }
static bool tree_eq_bool (const Tree& a, const Tree& b) noexcept {
    return a.data.as_bool == b.data.as_bool;
}
static bool tree_eq_number (const Tree& a, const Tree& b) noexcept {
    if (!a.floaty && !b.floaty) return a.data.as_i64 == b.data.as_i64;
    double av = a.floaty ? a.data.as_double : a.data.as_i64;
    double bv = b.floaty ? b.data.as_double : b.data.as_i64;
    return av == bv || (av != av && bv != bv);
}
static bool tree_eq_string (const Tree& a, const Tree& b) noexcept {
    return a.data.as_char_ptr == b.data.as_char_ptr
        || Str(a.data.as_char_ptr, a.size)
        == Str(b.data.as_char_ptr, b.size);
}
static bool tree_eq_array (const Tree& a, const Tree& b) noexcept {
     // Usually short-circuiting isn't worth it but array and especially
     // object comparison is pretty costly.
    return a.data.as_array_ptr == b.data.as_array_ptr
        || Slice<Tree>(a.data.as_array_ptr, a.size)
        == Slice<Tree>(b.data.as_array_ptr, b.size);
}

static bool tree_eq_object (const Tree& a, const Tree& b) noexcept {
    if (a.data.as_object_ptr == b.data.as_object_ptr) return true;
    if (a.size != b.size) return false;
     // This lets the compiler assume both loops run at least once.
    if (a.size == 0) return true;
     // The same attributes can be in different orders, so just compare each
     // attribute to each attribute for O(a.size * b.size).  In theory there are
     // faster algorithms, but they either require storing extra data in the
     // objects (which would break compatibility with AnyArray<TreePair>) or
     // would only be worth it for very large objects.
    auto ab = a.data.as_object_ptr;
    auto ae = ab + a.size;
    auto bb = b.data.as_object_ptr;
    auto be = bb + b.size;
    for (auto ap = ab; ap != ae; ap++) {
        for (auto bp = bb; bp != be; bp++) {
            if (ap->first == bp->first) {
                if (ap->second == bp->second) break;
                else return false;
            }
        }
    }
    return true;
}

static constexpr decltype(&tree_eq_false) tree_eqs [8] = {
    tree_eq_false,
    tree_eq_true,
    tree_eq_bool,
    tree_eq_number,
    tree_eq_string,
    tree_eq_array,
    tree_eq_object,
    tree_eq_false
};

} using namespace in;

bool operator == (const Tree& a, const Tree& b) noexcept {
    if (a.form != b.form) return false;
    expect(u32(a.form) < 8);
    return in::tree_eqs[u32(a.form)](a, b);
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Form,
    values(
        value("undefined", Form::Undefined),
        value("null", Form::Null),
        value("bool", Form::Bool),
        value("number", Form::Number),
        value("string", Form::String),
        value("array", Form::Array),
        value("object", Form::Object),
        value("error", Form::Error)
    )
)

 // Theoretically we could add support for attr and elem access to this, but
 // we'll save that for when we need it.
AYU_DESCRIBE(ayu::Tree,
    to_tree([](const Tree& v){ return v; }),
    from_tree([](Tree& v, const Tree& t){ v = t; return true; })
)

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

static tap::TestSet tests ("dirt/ayu/data/tree", []{
    using namespace tap;
    isnt(Tree(null), Tree(0), "Comparisons fail on different types");
    is(Tree(3), Tree(3.0), "Compare integers with floats");
    isnt(Tree(3), Tree(3.1), "Compare integers with floats (!=)");
    is(Tree(0.0/0.0), Tree(0.0/0.0), "Tree of NAN equals Tree of NAN");
    is(Str(Tree("asdfg")), "asdfg", "Round-trip strings");
    is(Str(Tree("qwertyuiop")), "qwertyuiop", "Round-trip long strings");
    throws_code<e_TreeWrongForm>([]{ int(Tree("0")); }, "Can't convert string to integer");
    try_is([]{ return int(Tree(3.0)); }, 3, "Convert floating to integer");
    try_is([]{ return double(Tree(3)); }, 3.0, "Convert integer to floating");
    throws_code<e_TreeCantRepresent>([]{
        int(Tree(3.5));
    }, "Can't convert 3.5 to integer");
    throws_code<e_TreeCantRepresent>([]{
        i8(Tree(1000));
    }, "Can't convert 1000 to i8");
    throws_code<e_TreeCantRepresent>([]{
        u8(Tree(-1));
    }, "Can't convert -1 to u8");
    is(Tree::array(Tree(3), Tree(4)), Tree::array(Tree(3), Tree(4)), "Compare arrays.");
    isnt(Tree::array(Tree(3), Tree(4)), Tree::array(Tree(4), Tree(3)), "Compare unequal arrays.");
    is(
        Tree::object(TreePair{"a", Tree(0)}, TreePair{"b", Tree(1)}),
        Tree::object(TreePair{"b", Tree(1)}, TreePair{"a", Tree(0)}),
        "AnyArray<TreePair> with same attributes in different order are equal"
    );
    isnt(
        Tree::object(TreePair{"a", Tree(0)}, TreePair{"b", Tree(1)}),
        Tree::object(TreePair{"b", Tree(1)}, TreePair{"a", Tree(0)}, TreePair{"c", Tree(3)}),
        "Extra attribute in second object makes it unequal"
    );
    done_testing();
});
#endif
