#include "to-tree.h"

#include "../reflection/descriptors.private.h"
#include "compound.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

static uint64 diagnostic_serialization = 0;

 // Putting these in a class just to avoid having to predeclare functions
struct TraverseToTree {
    static
    Tree start (const Reference& item, LocationRef loc) {
        PushBaseLocation pbl(*loc ? *loc : Location(item));
        Tree r;
        trav_start(item, loc, false, AccessMode::Read,
            [&r](const Traversal& trav)
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
            if (auto attrs = trav.desc->attrs()) {
                return use_attrs(r, trav, attrs);
            }
            else if (auto keys = trav.desc->keys_acr()) {
                auto f = trav.desc->attr_func()->f;
                return use_computed_attrs(r, trav, keys, f);
            }
            else never();
        }
        else if (trav.desc->preference() == Description::PREFER_ARRAY) {
            if (auto elems = trav.desc->elems()) {
                return use_elems(r, trav, elems);
            }
            else if (auto length = trav.desc->length_acr()) {
                auto f = trav.desc->elem_func()->f;
                return use_computed_elems(r, trav, length, f);
            }
            else never();
        }
        if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(r, trav, acr);
        }
        else fail(trav);
    }

///// STRATEGIES

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
    void use_attrs (
        Tree& r, const Traversal& trav, const AttrsDcrPrivate* attrs
    ) {
        auto object = UniqueArray<Pair<AnyString, Tree>>(
            Capacity(attrs->n_attrs)
        );
         // First just build the object as though none of the attrs are included
         // (but keep track if any are).
        bool any_included = false;
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (attr->acr()->attr_flags & AttrFlags::Invisible) continue;
            any_included |= !!(attr->acr()->attr_flags & AttrFlags::Include);

            trav_attr(trav, attr->acr(), attr->key, AccessMode::Read,
                [&object, attr](const Traversal& child)
            {
                Tree& value = object.emplace_back_expect_capacity(
                    attr->key, Tree()
                ).second;
                traverse(value, child);
                value.flags |= attr->acr()->tree_flags();
            });
        }
         // Then if there are included attrs, rebuild the object.
        if (any_included) {
             // Determine length for preallocation
            usize len = object.size();
            for (uint i = 0; i < attrs->n_attrs; i++) {
                if (attrs->attr(i)->acr()->attr_flags & AttrFlags::Include) {
                    len = len + object[i].second.length - 1;
                }
            }
             // Allocate
            auto new_object = decltype(object)(Capacity(len));
             // Selectively flatten
            for (uint i = 0; i < attrs->n_attrs; i++) {
                auto attr = attrs->attr(i);
                if (attr->acr()->attr_flags & AttrFlags::Include) {
                     // Consume the string too while we're here
                    AnyString(move(object[i].first));
                    Tree sub_tree = move(object[i].second);
                    if (sub_tree.rep != Rep::Object) {
                        raise(e_General, "Included item did not serialize to an object");
                    }
                    auto sub_object = TreeObject(move(sub_tree));
                    for (auto& pair : sub_object) {
                        new_object.emplace_back_expect_capacity(pair);
                    }
                }
                else {
                    new_object.emplace_back_expect_capacity(move(object[i]));
                }
            }
#ifndef NDEBUG
             // Old object's contents should be fully consumed
            for (auto& pair : object) {
                expect(!pair.first.owned());
                expect(!pair.second.has_value());
            }
#endif
            object.unsafe_set_size(0);
            object = move(new_object);
        }
         // This will check for duplicates in debug mode.
        new (&r) Tree(move(object));
    }

    NOINLINE static
    void use_computed_attrs (
        Tree& r, const Traversal& trav,
        const Accessor* keys_acr, AttrFunc<Mu>* f
    ) {
         // Get list of keys
        AnyArray<AnyString> keys;
        keys_acr->read(*trav.address, [&keys](Mu& v){
            new (&keys) AnyArray<AnyString>(
                reinterpret_cast<const AnyArray<AnyString>&>(v)
            );
        });
         // Now read value for each key
        auto object = TreeObject(Capacity(keys.size()));
        for (auto& key : keys) {
            auto ref = f(*trav.address, key);
            if (!ref) raise_AttrNotFound(trav.desc, key);
            trav_attr_func(trav, ref, f, key, AccessMode::Read,
                [&object, &key](const Traversal& child)
            {
                Tree& value = object.emplace_back_expect_capacity(
                    key, Tree()
                ).second;
                traverse(value, child);
            });
        }
        new (&r) Tree(move(object));
    }

    NOINLINE static
    void use_elems (
        Tree& r, const Traversal& trav, const ElemsDcrPrivate* elems
    ) {
        auto array = UniqueArray<Tree>(Capacity(elems->n_elems));
        for (uint i = 0; i < elems->n_elems; i++) {
            auto acr = elems->elem(i)->acr();
            trav_elem(trav, acr, i, AccessMode::Read,
                [&array](const Traversal& child)
            {
                 // This probably should never happen unless the elems are on
                 // the end and also optional.  TODO: Pop invisible elems off
                 // the end before allocating array.
                auto acr = static_cast<const AcrTraversal&>(child).acr;
                if (acr->attr_flags & AttrFlags::Invisible) return;
                Tree& elem = array.emplace_back_expect_capacity(Tree());
                traverse(elem, child);
                elem.flags |= acr->tree_flags();
            });
        }
        new (&r) Tree(move(array));
    }

    NOINLINE static
    void use_computed_elems (
        Tree& r, const Traversal& trav,
        const Accessor* length_acr, ElemFunc<Mu>* elem_func
    ) {
        usize len;
        length_acr->read(*trav.address, [&len](Mu& v){
            len = reinterpret_cast<const usize&>(v);
        });
        auto array = TreeArray(Capacity(len));
        for (usize i = 0; i < len; i++) {
            auto ref = elem_func(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.desc, i);
            trav_elem_func(trav, ref, elem_func, i, AccessMode::Read,
                [&array](const Traversal& child)
            {
                Tree& elem = array.emplace_back_expect_capacity(Tree());
                traverse(elem, child);
            });
        }
        new (&r) Tree(move(array));
    }

    NOINLINE static
    void use_delegate (
        Tree& r, const Traversal& trav, const Accessor* acr
    ) {
        trav_delegate(trav, acr, AccessMode::Read,
            [&r](const Traversal& child)
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
            "Item of type ", Type(trav.desc).name(), " does not support to_tree"
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

    [[noreturn, gnu::cold]] NOINLINE static
    void raise_KeysTypeInvalid(
        const Traversal& trav, Type keys_type
    ) {
        raise(e_KeysTypeInvalid, cat(
            "Item of type ", Type(trav.desc).name(),
            " gave keys() type ", keys_type.name(),
            " which does not serialize to an array of strings"
        ));
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
