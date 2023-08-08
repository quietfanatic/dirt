#include "../tree.h"

#include "../describe.h"
#include "../serialize-to-tree.h"

namespace ayu {
namespace in {

NOINLINE
void delete_Tree_data (TreeRef t) noexcept {
     // Delete by manifesting an array and letting its destructor run.
     // We're using Unique* instead of Shared* because we've already run down
     // the reference count.
    switch (t->rep) {
        case Rep::SharedString: {
            UniqueString::UnsafeConstructOwned(
                (char*)t->data.as_char_ptr, t->length
            );
            break;
        }
        case Rep::Array: {
            UniqueArray<Tree>::UnsafeConstructOwned(
                (Tree*)t->data.as_array_ptr, t->length
            );
            break;
        }
        case Rep::Object: {
            UniqueArray<TreePair>::UnsafeConstructOwned(
                (TreePair*)t->data.as_object_ptr, t->length
            );
            break;
        }
        case Rep::Error: {
            UniqueArray<std::exception_ptr>::UnsafeConstructOwned(
                (std::exception_ptr*)t->data.as_error_ptr, t->length
            );
            break;
        }
        default: never();
    }
}

void raise_TreeWrongForm (TreeRef t, Form form) {
    if (t->rep == Rep::Error) std::rethrow_exception(std::exception_ptr(*t));
     // TODO: require
    else if (t->form == form) never();
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

NOINLINE
bool tree_eq_str (const char* a, const char* b, usize s) {
    expect(s > 0);
    return memcmp(a, b, s) == 0;
}

NOINLINE
bool tree_eq_array (const Tree* a, const Tree* b, usize s) {
    expect(s > 0);
    for (usize i = 0; i < s; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

NOINLINE
bool tree_eq_object (TreeObjectSlice a, TreeObjectSlice b) {
    expect(a.size() == b.size());
    expect(a.size() > 0 && b.size() > 0);
    expect(a.data() != b.data());
     // Allow attributes to be in different orders
    for (auto& ap : a) {
        for (auto& bp : b) {
            if (ap.first == bp.first) {
                if (ap.second == bp.second) break;
                else return false;
            }
        }
    }
    return true;
}

} using namespace in;

NOINLINE
bool operator == (const Tree& a, const Tree& b) noexcept {
    if (a.rep != b.rep) {
         // Special case int/float comparisons
        if (a.rep == Rep::Int64 && b.rep == Rep::Double) {
            return a.data.as_int64 == b.data.as_double;
        }
        else if (a.rep == Rep::Double && b.rep == Rep::Int64) {
            return a.data.as_double == b.data.as_int64;
        }
         // Comparison between different-lifetime strings
        else if ((a.rep == Rep::StaticString && b.rep == Rep::SharedString)
              || (a.rep == Rep::SharedString && b.rep == Rep::StaticString)
        ) goto strs;
         // Otherwise different reps = different values.
        return false;
    }
    else switch (a.rep) {
        case Rep::Null: return true;
        case Rep::Bool:
        case Rep::Int64: return a.data.as_int64 == b.data.as_int64;
        case Rep::Double: {
            auto av = a.data.as_double;
            auto bv = b.data.as_double;
            return av == bv || (av != av && bv != bv);
        }
        case Rep::StaticString:
        case Rep::SharedString: goto strs;
        case Rep::Array: {
            if (a.length != b.length) return false;
            if (a.length == 0) return true;
            if (a.data.as_array_ptr == b.data.as_array_ptr) return true;
            return tree_eq_array(a.data.as_array_ptr, b.data.as_array_ptr, a.length);
        }
        case Rep::Object: {
            if (a.length != b.length) return false;
            if (a.length == 0) return true;
            if (a.data.as_object_ptr == b.data.as_object_ptr) return true;
            return tree_eq_object(
                TreeObjectSlice(a.data.as_object_ptr, a.length),
                TreeObjectSlice(b.data.as_object_ptr, b.length)
            );
        }
        case Rep::Error: return false;
        default: never();
    }
    strs: {
        if (a.length != b.length) return false;
        if (a.length == 0) return true;
        if (a.data.as_char_ptr == b.data.as_char_ptr) return true;
        return tree_eq_str(a.data.as_char_ptr, b.data.as_char_ptr, a.length);
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

static tap::TestSet tests ("dirt/ayu/tree", []{
    using namespace tap;
    isnt(Tree(null), Tree(0), "Comparisons fail on different types");
    is(Tree(3), Tree(3.0), "Compare integers with floats");
    isnt(Tree(3), Tree(3.1), "Compare integers with floats (!=)");
    is(Tree(0.0/0.0), Tree(0.0/0.0), "Tree of NAN equals Tree of NAN");
    is(Str(Tree("asdfg")), "asdfg", "Round-trip strings");
    is(Str(Tree("qwertyuiop")), "qwertyuiop", "Round-trip long strings");
    throws_code<e_TreeWrongForm>([]{ int(Tree("0")); }, "Can't convert string to integer");
    try_is<int>([]{ return int(Tree(3.0)); }, 3, "Convert floating to integer");
    try_is<double>([]{ return double(Tree(3)); }, 3.0, "Convert integer to floating");
    throws_code<e_TreeCantRepresent>([]{
        int(Tree(3.5));
    }, "Can't convert 3.5 to integer");
    throws_code<e_TreeCantRepresent>([]{
        int8(Tree(1000));
    }, "Can't convert 1000 to int8");
    throws_code<e_TreeCantRepresent>([]{
        uint8(Tree(-1));
    }, "Can't convert -1 to uint8");
    is(Tree(TreeArray{Tree(3), Tree(4)}), Tree(TreeArray{Tree(3), Tree(4)}), "Compare arrays.");
    isnt(Tree(TreeArray{Tree(3), Tree(4)}), Tree(TreeArray{Tree(4), Tree(3)}), "Compare unequal arrays.");
    is(
        Tree(TreeObject{TreePair{"a", Tree(0)}, TreePair{"b", Tree(1)}}),
        Tree(TreeObject{TreePair{"b", Tree(1)}, TreePair{"a", Tree(0)}}),
        "TreeObject with same attributes in different order are equal"
    );
    isnt(
        Tree(TreeObject{TreePair{"a", Tree(0)}, TreePair{"b", Tree(1)}}),
        Tree(TreeObject{TreePair{"b", Tree(1)}, TreePair{"a", Tree(0)}, TreePair{"c", Tree(3)}}),
        "Extra attribute in second object makes it unequal"
    );
    done_testing();
});
#endif
