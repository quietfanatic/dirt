#include "to-tree.h"

#include "compound.private.h"

namespace ayu {
namespace in {

static uint64 diagnostic_serialization = 0;

 // Putting these in a class just to avoid having to predeclare functions
struct TraverseToTree {
    static
    Tree start (const Reference& item, LocationRef loc) {
        PushBaseLocation pbl(*loc ? *loc : Location(item));
        Tree r;
        Traversal::start(
            item, loc, false, AccessMode::Read, [&r](const Traversal& trav)
        { traverse(r, trav); });
        return r;
    }

///// PICK STRATEGY

    NOINLINE static
    void traverse (Tree& r, const Traversal& trav) try {
         // The majority of items are [[likely]] to be atomic.
        if (auto to_tree = trav.desc->to_tree()) [[likely]] {
            use_to_tree(r, trav, to_tree->f);
        }
        else if (auto values = trav.desc->values()) {
            use_values(r, trav, values);
        }
        else no_value_match(r, trav);
    }
     // Unfortunately this exception handler prevents tail calling from this
     // function, but putting it anywhere else seems to perform worse.
    catch (...) { wrap_exception(r); }

    NOINLINE static
    void no_value_match (Tree& r, const Traversal& trav) {
        if (trav.desc->preference() == Description::PREFER_OBJECT) {
            use_object(r, trav);
        }
        else if (trav.desc->preference() == Description::PREFER_ARRAY) {
            use_array(r, trav);
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(r, trav, acr);
        }
        else fail(trav);
    }

///// EXECUTE STRATEGIES

     // NOINLINEing this seems to be worse, probably because traverse() already
     // has to do some stack setup for its try/catch.
    static
    void use_to_tree (
        Tree& r, const Traversal& trav, ToTreeFunc<Mu>* f
    ) {
        new (&r) Tree(f(*trav.address));
    }

    NOINLINE static
    void use_values (
        Tree& r, const Traversal& trav, const ValuesDcrPrivate* values
    ) {
        for (uint i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (values->compare(*trav.address, *value->get_value())) {
                new (&r) Tree(value->name);
                return;
            }
        }
        no_value_match(r, trav);
    }

    NOINLINE static
    void use_object (Tree& r, const Traversal& trav) {
        UniqueArray<AnyString> keys;
        trav_collect_keys(trav, keys);
        auto object = TreeObject(Capacity(keys.size()));
        for (auto& key : keys) {
            trav_attr(trav, key, AccessMode::Read,
                [&object, &key](const Traversal& child)
            {
                 // TODO: move this check to collect_keys
                if (child.op == ATTR &&
                    child.acr->attr_flags & AttrFlags::Invisible
                ) {
                    key = {};  // Consume key even though we didn't use it
                    return;
                }
                 // It's okay to move key even though the traversal stack has a
                 // pointer to it, because this is the last thing that happens
                 // before trav_attr returns.
                Tree& value = object.emplace_back_expect_capacity(
                    move(key), Tree()
                ).second;
                 // Recurse
                traverse(value, child);
                 // Get flags from acr
                if (child.op == ATTR) {
                    value.flags |= child.acr->tree_flags();
                }
            });
        }
         // All the keys have been consumed at this point, so skip the destructor
         // loop.
#ifndef NDEBUG
        for (auto& key : keys) expect(!key.owned());
#endif
        keys.unsafe_set_size(0);
        new (&r) Tree(move(object));
    }

    NOINLINE static
    void use_array (Tree& r, const Traversal& trav) {
        usize len = trav_get_length(trav);
        auto array = TreeArray(Capacity(len));
        for (usize i = 0; i < len; i++) {
            trav_elem(
                trav, i, AccessMode::Read, [&array](const Traversal& child)
            {
                if (child.op == ELEM &&
                    child.acr->attr_flags & AttrFlags::Invisible
                ) return;
                Tree& elem = array.emplace_back_expect_capacity(Tree());
                traverse(elem, child);
                if (child.op == ELEM) {
                    elem.flags |= child.acr->tree_flags();
                }
            });
        }
        new (&r) Tree(move(array));
    }

    NOINLINE static
    void use_delegate (
        Tree& r, const Traversal& trav, const Accessor* acr
    ) {
        trav.follow_delegate(
            acr, AccessMode::Read, [&r](const Traversal& child)
        { traverse(r, child); });
        r.flags |= acr->tree_flags();
    }

///// ERRORS

    [[noreturn, gnu::cold]] NOINLINE static
    void fail (const Traversal& trav) {
        if (trav.desc->values()) {
            raise(e_ToTreeValueNotFound, cat(
                "No value for type ", Type(trav.desc).name(),
                " matches the item's value"
            ));
        }
        else raise(e_ToTreeNotSupported, cat(
            "Item of type ", Type(trav.desc).name(), " does not support to_tree."
        ));
    }

    [[gnu::cold]] NOINLINE static
    void wrap_exception (Tree& r) {
        auto ex = std::current_exception();
        if (diagnostic_serialization) {
            new (&r) Tree(move(ex));
        }
        else std::rethrow_exception(move(ex));
    }
};

} using namespace in;

Tree item_to_tree (const Reference& item, LocationRef loc) {
    return TraverseToTree::start(item, loc);
}

DiagnosticSerialization::DiagnosticSerialization () {
    diagnostic_serialization += 1;
}
DiagnosticSerialization::~DiagnosticSerialization () {
    expect(diagnostic_serialization > 0);
    diagnostic_serialization -= 1;
}

} using namespace ayu;
