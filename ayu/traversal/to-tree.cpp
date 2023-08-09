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
            if (auto attrs = trav.desc->attrs()) {
                use_attrs(r, trav, attrs);
            }
            else use_computed_attrs(
                r, trav, trav.desc->keys_acr(), trav.desc->attr_func()->f
            );
        }
        else if (trav.desc->preference() == Description::PREFER_ARRAY) {
            if (auto elems = trav.desc->elems()) {
                use_elems(r, trav, elems);
            }
            else use_computed_elems(
                r, trav, trav.desc->length_acr(), trav.desc->elem_func()->f
            );
        }
        else if (auto acr = trav.desc->delegate_acr()) {
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
         // This may have to reallocate if there are included attrs.
        auto object = TreeObject(Capacity(attrs->n_attrs));
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (attr->acr()->attr_flags & AttrFlags::Invisible) continue;

            trav.follow_attr(
                attr->acr(), attr->key, AccessMode::Read,
                [&object, attr](const Traversal& child)
            {
                if (attr->acr()->attr_flags & AttrFlags::Include) {
                     // For included attrs, just serialize the whole thing and
                     // then append to the current object.
                    Tree sub;
                    traverse(sub, child);
                     // TODO: add ToTreeFlags::PreferObject
                    if (sub.rep != Rep::Object) {
                        raise(e_General, "Included item did not serialize to an object");
                    }
                    auto sub_object = TreeObject(sub);
                    object.reserve_plenty(object.size() + sub_object.size());
                    for (auto& pair : sub_object) {
                        object.emplace_back_expect_capacity(pair);
                    }
                }
                else {
                     // Can't emplace_back_expect_capacity because an included
                     // attr may have interfered with the remaining capacity.
                    Tree& value = object.emplace_back(attr->key, Tree()).second;
                    traverse(value, child);
                    value.flags |= attr->acr()->tree_flags();
                }
            });
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
        Type keys_type = keys_acr->type(trav.address);
        if (keys_type == Type::CppType<AnyArray<AnyString>>()) {
            get_keys_AnyArray(keys, trav, keys_acr);
        }
        else [[unlikely]] {
            get_keys_generic(keys, trav, keys_acr, keys_type);
        }
         // Now get value for each key
        auto object = TreeObject(Capacity(keys.size()));
        for (auto& key : keys) {
            auto ref = f(*trav.address, key);
            if (!ref) raise_AttrNotFound(trav.desc, key);
            trav.follow_attr_func(
                ref, f, key, AccessMode::Read,
                [&object, &key](const Traversal& child)
            {
                 // It's okay to move key even though the traversal stack has a
                 // pointer to it, because this is the last thing that happens
                 // before trav.follow_attr_func returns.
                Tree& value = object.emplace_back_expect_capacity(
                    move(key), Tree()
                ).second;
                traverse(value, child);
            });
        }
        new (&r) Tree(move(object));
    }

    static
    void get_keys_AnyArray (
        AnyArray<AnyString>& keys, const Traversal& trav,
        const Accessor* keys_acr
    ) {
        keys_acr->read(*trav.address, [&keys](Mu& v){
            new (&keys) AnyArray<AnyString>(
                reinterpret_cast<const AnyArray<AnyString>&>(v)
            );
        });
    }

    static
    void get_keys_generic (
        AnyArray<AnyString>& keys, const Traversal& trav,
        const Accessor* keys_acr, Type keys_type
    ) {
        keys_acr->read(*trav.address, [&keys, &trav, keys_type](Mu& v){
             // We might be able to optimize this more, but it's not that
             // important.
            auto keys_tree = item_to_tree(Pointer(keys_type, &v));
            if (keys_tree.rep != Rep::Array) {
                raise_KeysTypeInvalid(trav, keys_type);
            }
            auto array = TreeArraySlice(keys_tree);
            new (&keys) AnyArray<AnyString>(Capacity(array.size()));
            for (const Tree& key : array) {
                if (key.form != Form::String) {
                    raise_KeysTypeInvalid(trav, keys_type);
                }
                keys.emplace_back_expect_capacity(AnyString(move(key)));
            }
        });
    }

    NOINLINE static
    void use_elems (
        Tree& r, const Traversal& trav, const ElemsDcrPrivate* elems
    ) {
        auto array = TreeArray(Capacity(elems->n_elems));
        for (uint i = 0; i < elems->n_elems; i++) {
            auto acr = elems->elem(i)->acr();
            trav.follow_elem(
                acr, i, AccessMode::Read, [&array](const Traversal& child)
            {
                 // This probably should never happen unless the elems are on
                 // the end and also optional.  TODO: Pop invisible elems off
                 // the end before allocating array.
                if (child.acr->attr_flags & AttrFlags::Invisible) return;
                Tree& elem = array.emplace_back_expect_capacity(Tree());
                traverse(elem, child);
                elem.flags |= child.acr->tree_flags();
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
            trav.follow_elem_func(
                ref, elem_func, i, AccessMode::Read,
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
