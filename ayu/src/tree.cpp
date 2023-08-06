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

} using namespace in;

bool operator == (TreeRef a, TreeRef b) noexcept {
    if (a->rep != b->rep) {
         // Special case int/float comparisons
        if (a->rep == Rep::Int64 && b->rep == Rep::Double) {
            return int64(*a) == double(*b);
        }
        else if (a->rep == Rep::Double && b->rep == Rep::Int64) {
            return double(*a) == int64(*b);
        }
         // Comparison between different-lifetime strings
        else if ((a->rep == Rep::StaticString && b->rep == Rep::SharedString)
              || (a->rep == Rep::SharedString && b->rep == Rep::StaticString)
        ) {
            return Str(*a) == Str(*b);
        }
         // Otherwise different reps = different values.
        return false;
    }
    else switch (a->rep) {
        case Rep::Null: return true;
        case Rep::Bool: return bool(*a) == bool(*b);
        case Rep::Int64: return int64(*a) == int64(*b);
        case Rep::Double: {
            auto av = double(*a);
            auto bv = double(*b);
            return av == bv || (av != av && bv != bv);
        }
        case Rep::StaticString:
        case Rep::SharedString: {
            return Str(*a) == Str(*b);
        }
        case Rep::Array: {
            return TreeArraySlice(*a) == TreeArraySlice(*b);
        }
        case Rep::Object: {
             // Allow attributes to be in different orders
            auto ao = TreeObjectSlice(*a);
            auto bo = TreeObjectSlice(*b);
            if (ao.size() != bo.size()) return false;
            else for (auto& ap : ao) {
                for (auto& bp : bo) {
                    if (ap.first == bp.first) {
                        if (ap.second == bp.second) break;
                        else return false;
                    }
                }
            }
            return true;
        }
        case Rep::Error: return false;
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
