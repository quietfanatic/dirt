#include "tree.h"

#include "../reflection/describe.h"
#include "../traversal/to-tree.h"

namespace ayu {
namespace in {

NOINLINE
void delete_Tree_data (TreeRef t) noexcept {
     // Manually delete all the elements.  We can't call UniqueArray<*>'s
     // destructor because we've already run the reference count down to 0, and
     // it debug-asserts that the reference count is 1.
    expect(t->meta & 1);
    switch (t->form) {
        case Form::String: {
            SharableBuffer<const char>::deallocate(t->data.as_char_ptr);
            break;
        }
        case Form::Array: {
            for (auto& e : Slice<Tree>(*t)) {
                e.~Tree();
            }
            SharableBuffer<const Tree>::deallocate(t->data.as_array_ptr);
            break;
        }
        case Form::Object: {
            for (auto& p : Slice<TreePair>(*t)) {
                p.~TreePair();
            }
            SharableBuffer<const TreePair>::deallocate(t->data.as_object_ptr);
            break;
        }
        case Form::Error: {
            t->data.as_error_ptr->~exception_ptr();
            SharableBuffer<const std::exception_ptr>::deallocate(t->data.as_error_ptr);
            break;
        }
        default: never();
    }
}

void raise_TreeWrongForm (TreeRef t, Form form) {
    if (t->form == Form::Error) std::rethrow_exception(std::exception_ptr(*t));
    else raise(e_TreeWrongForm, cat(
        "Tried to use tree of form ", item_to_string(&t->form),
        " as form ", item_to_string(&form)
    ));
}

void raise_TreeCantRepresent (StaticString type_name, TreeRef t) {
    raise(e_TreeCantRepresent, cat(
        "Can't represent type ", type_name, " with value ", tree_to_string(t)
    ));
}

 // NOINLINE these so they don't make operator== push and pop a bunch of
 // registers.
NOINLINE
bool tree_eq_array (const Tree* a, const Tree* b, usize s) {
    expect(s > 0);
    for (auto ae = a + s; a != ae; a++, b++) {
        if (*a != *b) return false;
    }
    return true;
}

NOINLINE
bool tree_eq_object (const TreePair* a, const TreePair* b, usize s) {
    expect(s > 0);
     // Allow attributes to be in different orders
    auto ae = a + s;
    auto be = b + s;
    for (auto ap = a; ap != ae; ap++) {
        for (auto bp = b; bp != be; bp++) {
            if (ap->first == bp->first) {
                if (ap->second == bp->second) break;
                else return false;
            }
        }
    }
    return true;
}

} using namespace in;

NOINLINE
bool operator == (const Tree& a, const Tree& b) noexcept {
    if (a.form != b.form) return false;
    else switch (a.form) {
        case Form::Null: return true;
        case Form::Bool: return a.data.as_bool == b.data.as_bool;
        case Form::Number: {
            if (a.meta) {
                if (b.meta) {
                    auto av = a.data.as_double;
                    auto bv = b.data.as_double;
                    return av == bv || (av != av && bv != bv);
                }
                else return a.data.as_double == b.data.as_int64;
            }
            else if (b.meta) {
                return a.data.as_int64 == b.data.as_double;
            }
            else return a.data.as_int64 == b.data.as_int64;
        }
        case Form::String: {
            return Str(a.data.as_char_ptr, a.meta >> 1) ==
                   Str(b.data.as_char_ptr, b.meta >> 1);
        }
        case Form::Array: {
            if (a.meta >> 1 != b.meta >> 1) return false;
            if (a.meta >> 1 == 0) return true;
             // Usually short-circuiting isn't worth it but array and especially
             // object comparison is pretty costly.
            if (a.data.as_array_ptr == b.data.as_array_ptr) return true;
            return tree_eq_array(
                a.data.as_array_ptr, b.data.as_array_ptr, a.meta >> 1
            );
        }
        case Form::Object: {
            if (a.meta >> 1 != b.meta >> 1) return false;
            if (a.meta >> 1 == 0) return true;
            if (a.data.as_object_ptr == b.data.as_object_ptr) return true;
            return tree_eq_object(
                a.data.as_object_ptr, b.data.as_object_ptr, a.meta >> 1
            );
        }
        case Form::Error: return false;
        default: never();
    }
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
    from_tree([](Tree& v, const Tree& t){ v = t; })
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
        int8(Tree(1000));
    }, "Can't convert 1000 to int8");
    throws_code<e_TreeCantRepresent>([]{
        uint8(Tree(-1));
    }, "Can't convert -1 to uint8");
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
