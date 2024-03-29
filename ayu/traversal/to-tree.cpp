#include "to-tree.h"

#include "../reflection/descriptors.private.h"
#include "compound.h"
#include "scan.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

static uint64 diagnostic_serialization = 0;

 // Putting these in a class just to avoid having to predeclare functions
struct TraverseToTree {
     // NOINLINE this because it generates a lot of code with the trav_start
    NOINLINE static
    Tree start (const Reference& item, LocationRef loc) {
        plog("to_tree start");
        PushBaseLocation pbl(loc ? loc : LocationRef(SharedLocation(item)));
        KeepLocationCache klc;
        Tree r;
        trav_start(item, loc, false, AccessMode::Read,
            [&r](const Traversal& child){ traverse(r, child); }
        );
        plog("to_tree end");
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
    catch (...) { if (!wrap_exception(r)) throw; }

    NOINLINE static
    void no_value_match (Tree& r, const Traversal& trav) {
        if (trav.desc->preference() == DescFlags::PreferObject) {
            if (auto keys = trav.desc->keys_acr()) {
                return use_computed_attrs(r, trav, keys);
            }
            else {
                return use_attrs(r, trav);
            }
        }
        else if (trav.desc->preference() == DescFlags::PreferArray) {
            if (auto length = trav.desc->length_acr()) {
                if (!!(trav.desc->flags & DescFlags::ElemsContiguous)) {
                    return use_contiguous_elems(r, trav, length);
                }
                else {
                    return use_computed_elems(r, trav, length);
                }
            }
            else {
                return use_elems(r, trav);
            }
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(r, trav, acr);
        }
        else fail(trav);
    }

///// STRATEGIES

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
        Tree& r, const Traversal& trav
    ) {
        expect(trav.desc->attrs_offset);
        auto attrs = trav.desc->attrs();
        auto object = UniqueArray<TreePair>(Capacity(attrs->n_attrs));
         // First just build the object as though none of the attrs are included
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (!!(attr->acr()->attr_flags & AttrFlags::Invisible)) continue;

            Tree& value = object.emplace_back_expect_capacity(
                attr->key, Tree()
            ).second;
            trav_attr(trav, attr->acr(), attr->key, AccessMode::Read,
                [&value](const Traversal& child){ traverse(value, child); }
            );
            value.flags |= attr->acr()->tree_flags();
        }
         // Then if there are included or collapsed attrs, rebuild the object
         // while flattening them.
        if (!!(trav.desc->flags & DescFlags::AttrsNeedRebuild)) {
             // Determine length for preallocation
            usize len = object.size();
            for (uint i = 0; i < attrs->n_attrs; i++) {
                auto flags = attrs->attr(i)->acr()->attr_flags;
                 // Ignore CollapseEmpty; it can only decrease the length by 1,
                 // and the calculation for whether to do it is a bit
                 // complicated, so I'd rather just overallocate.
                if (!!(flags &
                    (AttrFlags::Include|AttrFlags::CollapseOptional)
                )) {
                     // This works for both include and collapse_optional
                    len = len + (object[i].second.meta >> 1) - 1;
                }
            }
             // Allocate
            auto new_object = decltype(object)(Capacity(len));
             // Selectively flatten
            for (uint i = 0; i < attrs->n_attrs; i++) {
                auto attr = attrs->attr(i);
                auto flags = attr->acr()->attr_flags;
                auto key = move(object[i].first);
                Tree value = move(object[i].second);
                if (!!(flags & AttrFlags::Include)) {
                    if (value.form != Form::Object) {
                        raise(e_General,
                            "Included item did not serialize to an object"
                        );
                    }
                     // DON'T consume sub object because it could be shared.
                    for (auto& pair : AnyArray<TreePair>(move(value))) {
                        new_object.emplace_back_expect_capacity(pair);
                    }
                }
                else if (!!(flags & AttrFlags::CollapseEmpty)) {
                    bool compound = value.form == Form::Array ||
                                    value.form == Form::Object;
                    if (!compound || value.meta >> 1) {
                        new_object.emplace_back_expect_capacity(
                            move(key), move(value)
                        );
                    }
                    else { } // Drop the attr
                }
                else if (!!(flags & AttrFlags::CollapseOptional)) {
                    if (value.form != Form::Array || (value.meta >> 1) > 1) {
                        raise(e_General,
                            "Attribute with collapse_optional did not "
                            "serialize to an array of 0 or 1 elements"
                        );
                    }
                    if (auto a = AnyArray<Tree>(move(value))) {
                        new_object.emplace_back_expect_capacity(
                            move(key), a[0]
                        );
                    }
                    else { } // Drop the attr
                }
                else {
                    new_object.emplace_back_expect_capacity(
                        move(key), move(value)
                    );
                }
            }
             // Old object's contents should be fully consumed so skip the
             // destructor loop (but verify in debug mode).
#ifndef NDEBUG
            for (auto& pair : object) {
                expect(!pair.first.owned());
                expect(!(pair.second.meta & 1));
            }
#endif
            SharableBuffer<TreePair>::deallocate(object.impl.data);
            new (&object) UniqueArray<TreePair>(move(new_object));
        }
         // This will check for duplicates in debug mode.
        new (&r) Tree(move(object));
    }

    NOINLINE static
    void use_computed_attrs (
        Tree& r, const Traversal& trav,
        const Accessor* keys_acr
    ) {
         // Get list of keys
        AnyArray<AnyString> keys;
        keys_acr->read(*trav.address, CallbackRef<void(Mu&)>(
            keys, [](AnyArray<AnyString>& keys, Mu& v)
        {
            new (&keys) AnyArray<AnyString>(
                reinterpret_cast<const AnyArray<AnyString>&>(v)
            );
        }));
         // Now read value for each key
        auto object = UniqueArray<TreePair>(Capacity(keys.size()));
        expect(trav.desc->computed_attrs_offset);
        auto f = trav.desc->computed_attrs()->f;
        for (auto& key : keys) {
            auto ref = f(*trav.address, key);
            if (!ref) raise_AttrNotFound(trav.desc, key);
            auto& [_, value] = object.emplace_back_expect_capacity(key, Tree());
            trav_computed_attr(trav, ref, f, key, AccessMode::Read,
                [&value](const Traversal& child){ traverse(value, child); }
            );
        }
        new (&r) Tree(move(object));
    }

    NOINLINE static
    void use_elems (
        Tree& r, const Traversal& trav
    ) {
        expect(trav.desc->elems_offset);
        auto elems = trav.desc->elems();
        auto len = elems->chop_flag(AttrFlags::Invisible);
        auto array = UniqueArray<Tree>(Capacity(len));
        for (uint i = 0; i < len; i++) {
            auto acr = elems->elem(i)->acr();
            Tree& elem = array.emplace_back_expect_capacity(Tree());
            trav_elem(trav, acr, i, AccessMode::Read,
                [&elem](const Traversal& child){ traverse(elem, child); }
            );
            elem.flags |= acr->tree_flags();
        }
        new (&r) Tree(move(array));
    }

    NOINLINE static
    void use_contiguous_elems (
        Tree& r, const Traversal& trav,
        const Accessor* length_acr
    ) {
        usize len;
        length_acr->read(*trav.address,
            CallbackRef<void(Mu& v)>(len, [](usize& len, Mu& v){
                len = reinterpret_cast<const usize&>(v);
            })
        );
        auto array = UniqueArray<Tree>(Capacity(len));
        if (len) {
            expect(trav.desc->contiguous_elems_offset);
            auto f = trav.desc->contiguous_elems()->f;
            auto ptr = f(*trav.address);
            auto child_desc = DescriptionPrivate::get(ptr.type);
            for (usize i = 0; i < len; i++) {
                auto& elem = array.emplace_back_expect_capacity();
                trav_contiguous_elem(trav, ptr, f, i, AccessMode::Read,
                    [&elem](const Traversal& child){ traverse(elem, child); }
                );
                ptr.address = (Mu*)((char*)ptr.address + child_desc->cpp_size);
            }
        }
        new (&r) Tree(move(array));
    }

    NOINLINE static
    void use_computed_elems (
        Tree& r, const Traversal& trav,
        const Accessor* length_acr
    ) {
        usize len;
        length_acr->read(*trav.address,
            CallbackRef<void(Mu& v)>(len, [](usize& len, Mu& v){
                len = reinterpret_cast<const usize&>(v);
            })
        );
        auto array = UniqueArray<Tree>(Capacity(len));
        expect(trav.desc->computed_elems_offset);
        auto f = trav.desc->computed_elems()->f;
        for (usize i = 0; i < len; i++) {
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.desc, i);
            auto& elem = array.emplace_back_expect_capacity();
            trav_computed_elem(trav, ref, f, i, AccessMode::Read,
                [&elem](const Traversal& child){ traverse(elem, child); }
            );
        }
        new (&r) Tree(move(array));
    }

    NOINLINE static
    void use_delegate (
        Tree& r, const Traversal& trav, const Accessor* acr
    ) {
        trav_delegate(trav, acr, AccessMode::Read,
            [&r](const Traversal& child){ traverse(r, child); }
        );
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

     // NOINLINE this so its stack requirements don't get applied to traverse()
    [[gnu::cold]] NOINLINE static
    bool wrap_exception (Tree& r) {
        if (diagnostic_serialization) {
            new (&r) Tree(std::current_exception());
            return true;
        }
        else return false;
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
