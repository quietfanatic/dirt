#include "../serialize-to-tree.h"

#include "serialize-compound-private.h"

namespace ayu {
namespace in {

static uint64 diagnostic_serialization = 0;

Tree ser_to_tree (const Traversal& trav) try {
     // The majority of items are [[likely]] to be atomic.
    if (auto to_tree = trav.desc->to_tree()) [[likely]] {
        return to_tree->f(*trav.address);
    }
    if (auto values = trav.desc->values()) {
        for (uint i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (values->compare(*trav.address, *value->get_value())) {
                return value->name;
            }
        }
    }
    if (trav.desc->preference() == Description::PREFER_OBJECT) {
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
                Tree value = ser_to_tree(child);
                 // Get flags from acr
                if (child.op == ATTR) {
                    value.flags |= child.acr->tree_flags();
                }
                 // It's okay to move key even though the traversal stack has a
                 // pointer to it, because this is the last thing that happens
                 // before ser_attr returns.
                object.emplace_back_expect_capacity(move(key), move(value));
            });
        }
        return Tree(move(object));
    }
    if (trav.desc->preference() == Description::PREFER_ARRAY) {
        usize len = ser_get_length(trav);
        auto array = TreeArray(Capacity(len));
        for (usize i = 0; i < len; i++) {
            ser_elem(
                trav, i, AccessMode::Read, [&array](const Traversal& child)
            {
                if (child.op == ELEM &&
                    child.acr->attr_flags & AttrFlags::Invisible
                ) return;
                Tree elem = ser_to_tree(child);
                if (child.op == ELEM) {
                    elem.flags |= child.acr->tree_flags();
                }
                array.emplace_back_expect_capacity(move(elem));
            });
        }
        return Tree(move(array));
    }
    if (auto acr = trav.desc->delegate_acr()) {
        Tree r;
        trav.follow_delegate(
            acr, AccessMode::Read, [&r](const Traversal& child)
        { new (&r) Tree(ser_to_tree(child)); });
        r.flags |= acr->tree_flags();
        return r;
    }
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
catch (const std::exception&) {
     // TODO also check std::uncaught_exceptions
    if (diagnostic_serialization) {
        return Tree(std::current_exception());
    }
    else throw;
}

} using namespace in;

Tree item_to_tree (const Reference& item, LocationRef loc) {
    PushBaseLocation pbl(*loc ? *loc : Location(item));
    Tree r;
    Traversal::start(
        item, loc, false, AccessMode::Read, [&r](const Traversal& trav)
    { new (&r) Tree(ser_to_tree(trav)); });
    return r;
}

DiagnosticSerialization::DiagnosticSerialization () {
    diagnostic_serialization += 1;
}
DiagnosticSerialization::~DiagnosticSerialization () {
    expect(diagnostic_serialization > 0);
    diagnostic_serialization -= 1;
}

} using namespace ayu;
