#include "../serialize-to-tree.h"

#include "serialize-compound-private.h"

namespace ayu {
namespace in {

void ser_to_tree (Tree&, const Traversal&);
void ser_to_tree_values (Tree&, const Traversal&, const ValuesDcrPrivate*);
void ser_to_tree_after_values (Tree&, const Traversal&);
void ser_to_tree_object (Tree&, const Traversal&);
void ser_to_tree_array (Tree&, const Traversal&);
void ser_to_tree_delegate (Tree&, const Traversal&, const Accessor*);
[[noreturn]] void ser_to_tree_error (Tree&, const Traversal&);
void ser_to_tree_wrap_exception (Tree&);

static uint64 diagnostic_serialization = 0;

} using namespace in;

Tree item_to_tree (const Reference& item, LocationRef loc) {
    PushBaseLocation pbl(*loc ? *loc : Location(item));
    Tree r;
    Traversal::start(
        item, loc, false, AccessMode::Read, [&r](const Traversal& trav)
    { ser_to_tree(r, trav); });
    return r;
}

DiagnosticSerialization::DiagnosticSerialization () {
    diagnostic_serialization += 1;
}
DiagnosticSerialization::~DiagnosticSerialization () {
    expect(diagnostic_serialization > 0);
    diagnostic_serialization -= 1;
}

namespace in {

NOINLINE
void ser_to_tree (Tree& r, const Traversal& trav) try {
     // The majority of items are [[likely]] to be atomic.
    if (auto to_tree = trav.desc->to_tree()) [[likely]] {
        new (&r) Tree(to_tree->f(*trav.address));
        return;
    }
    if (auto values = trav.desc->values()) {
        ser_to_tree_values(r, trav, values);
    }
    else ser_to_tree_after_values(r, trav);
}
catch (...) { ser_to_tree_wrap_exception(r); }

NOINLINE
void ser_to_tree_values (
    Tree& r, const Traversal& trav, const ValuesDcrPrivate* values
) {
    for (uint i = 0; i < values->n_values; i++) {
        auto value = values->value(i);
        if (values->compare(*trav.address, *value->get_value())) {
            new (&r) Tree(value->name);
            return;
        }
    }
    ser_to_tree_after_values(r, trav);
}

NOINLINE
void ser_to_tree_after_values (Tree& r, const Traversal& trav) {
    if (trav.desc->preference() == Description::PREFER_OBJECT) {
        ser_to_tree_object(r, trav);
    }
    else if (trav.desc->preference() == Description::PREFER_ARRAY) {
        ser_to_tree_array(r, trav);
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        ser_to_tree_delegate(r, trav, acr);
    }
    else ser_to_tree_error(r, trav);
}

NOINLINE
void ser_to_tree_object (Tree& r, const Traversal& trav) {
    UniqueArray<AnyString> keys;
    ser_collect_keys(trav, keys);
    auto object = TreeObject(Capacity(keys.size()));
    for (auto& key : keys) {
        ser_attr(trav, key, AccessMode::Read,
            [&object, &key](const Traversal& child)
        {
            if (child.op == ATTR &&
                child.acr->attr_flags & AttrFlags::Invisible
            ) return;
             // It's okay to move key even though the traversal stack has a
             // pointer to it, because this is the last thing that happens
             // before ser_attr returns.
            Tree& value = object.emplace_back_expect_capacity(
                move(key), Tree()
            ).second;
            ser_to_tree(value, child);
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
    SharableBuffer<AnyString>::deallocate(keys.data());
    keys.unsafe_set_empty();
    new (&r) Tree(move(object));
}

NOINLINE
void ser_to_tree_array (Tree& r, const Traversal& trav) {
    usize len = ser_get_length(trav);
    auto array = TreeArray(Capacity(len));
    for (usize i = 0; i < len; i++) {
        ser_elem(
            trav, i, AccessMode::Read, [&array](const Traversal& child)
        {
            if (child.op == ELEM &&
                child.acr->attr_flags & AttrFlags::Invisible
            ) return;
            Tree& elem = array.emplace_back_expect_capacity(Tree());
            ser_to_tree(elem, child);
            if (child.op == ELEM) {
                elem.flags |= child.acr->tree_flags();
            }
        });
    }
    new (&r) Tree(move(array));
}

NOINLINE
void ser_to_tree_delegate (
    Tree& r, const Traversal& trav, const Accessor* acr
) {
    trav.follow_delegate(
        acr, AccessMode::Read, [&r](const Traversal& child)
    { ser_to_tree(r, child); });
    r.flags |= acr->tree_flags();
}

[[gnu::cold]] NOINLINE
void ser_to_tree_error (
    Tree&, const Traversal& trav
) {
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

[[gnu::cold]] NOINLINE
void ser_to_tree_wrap_exception (Tree& r) {
    auto ex = std::current_exception();
    if (diagnostic_serialization) {
        new (&r) Tree(move(ex));
    }
    else std::rethrow_exception(move(ex));
}

} using namespace in;

} using namespace ayu;
