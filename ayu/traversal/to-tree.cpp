#include "to-tree.h"

#include "../reflection/descriptors.private.h"
#include "compound.h"
#include "scan.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

static uint64 diagnostic_serialization = 0;

 // The subtypes in traversal.private.h build the struct to the right (as is
 // normal), so we'll build it to the left instead.
struct ToTreeTraversalHead {
    Tree* dest;
};
 // If you know the left side of a Traversal but not the right side, you should
 // be able to static_cast to ToTreeTraversal<>.  This might technically be
 // forbidden by the spec, but it should work, assuming nothing messes with the
 // alignment.
template <class T = Traversal>
struct ToTreeTraversal : ToTreeTraversalHead, T { };

 // These are only in a struct so we don't have to predeclare functions
struct TraverseToTree {

     // NOINLINE this because it generates a lot of code with trav_start
    NOINLINE static
    Tree start (const AnyRef& item, LocationRef loc) {
        plog("to_tree start");
        PushBaseLocation pbl(loc ? loc : LocationRef(SharedLocation(item)));
        KeepLocationCache klc;
        Tree dest;
        ToTreeTraversal<StartTraversal> child;
        child.dest = &dest;
        trav_start<visit>(
            child, item, loc, AccessMode::Read
        );
        plog("to_tree end");
        return dest;
    }

///// PICK STRATEGY

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const ToTreeTraversal<>&>(tr);
        try {
             // The majority of items are [[likely]] to be atomic.
            if (auto to_tree = trav.desc->to_tree()) [[likely]] {
                use_to_tree(trav, to_tree->f);
            }
            else if (auto values = trav.desc->values()) {
                use_values(trav, values);
            }
            else no_value_match(trav);
        }
         // Unfortunately this exception handler prevents tail calling from this
         // function, but putting it anywhere else seems to perform worse.
        catch (...) { if (!wrap_exception(trav)) throw; }
    }

    NOINLINE static
    void no_value_match (const ToTreeTraversal<>& trav) {
        if (trav.desc->preference() == DescFlags::PreferObject) {
            if (auto keys = trav.desc->keys_acr()) {
                return use_computed_attrs(trav, keys);
            }
            else if (auto attrs = trav.desc->attrs()) {
                return use_attrs(trav, attrs);
            }
            else never();
        }
        else if (trav.desc->preference() == DescFlags::PreferArray) {
            if (auto length = trav.desc->length_acr()) {
                if (!!(trav.desc->flags & DescFlags::ElemsContiguous)) {
                    return use_contiguous_elems(trav, length);
                }
                else {
                    return use_computed_elems(trav, length);
                }
            }
            else if (auto elems = trav.desc->elems()) {
                return use_elems(trav, elems);
            }
            else never();
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(trav, acr);
        }
        else fail(trav);
    }

///// STRATEGIES

    static
    void use_to_tree (const ToTreeTraversal<>& trav, ToTreeFunc<Mu>* f) {
        new (trav.dest) Tree(f(*trav.address));
    }

    NOINLINE static
    void use_values (
        const ToTreeTraversal<>& trav, const ValuesDcrPrivate* values
    ) {
        for (uint i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (values->compare(*trav.address, *value->get_value())) {
                new (trav.dest) Tree(value->name);
                return;
            }
        }
        no_value_match(trav);
    }

    NOINLINE static
    void use_attrs (
        const ToTreeTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
        auto object = UniqueArray<TreePair>(Capacity(attrs->n_attrs));
         // First just build the object as though none of the attrs are included
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (!!(attr->acr()->attr_flags & AttrFlags::Invisible)) continue;

            ToTreeTraversal<AttrTraversal> child;

            child.dest = &object.emplace_back_expect_capacity(
                attr->key, Tree()
            ).second;
            trav_attr<visit>(
                child, trav, attr->acr(), attr->key, AccessMode::Read
            );
            child.dest->flags |= child.acr->tree_flags();
        }
         // Then if there are included or collapsed attrs, rebuild the object
         // while flattening them.
        if (!!(trav.desc->flags & DescFlags::AttrsNeedRebuild)) {
             // Determine length for preallocation
            usize len = object.size();
            for (uint i = 0; i < attrs->n_attrs; i++) {
                auto flags = attrs->attr(i)->acr()->attr_flags;
                 // Ignore HasDefault; it can only decrease the length by 1, and
                 // checking whether it does requires comparing Trees, so I'd
                 // rather just overallocate.
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
                else if (const Tree* def = attr->default_value()) {
                    if (value != *def) {
                        new_object.emplace_back_expect_capacity(
                            move(key), move(value)
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
        new (trav.dest) Tree(move(object));
    }

    NOINLINE static
    void use_computed_attrs (
        const ToTreeTraversal<>& trav, const Accessor* keys_acr
    ) {
         // Populate keys
        UniqueArray<TreePair> object;
        keys_acr->read(*trav.address,
            AccessCB(object, [](auto& object, AnyPtr v, bool)
        {
            require_readable_keys(v.type);
            auto& keys = reinterpret_cast<
                const AnyArray<AnyString>&
            >(*v.address);
            expect(!object);
            object = UniqueArray<TreePair>(keys.size(), [&](usize i){
                return TreePair{keys[i], Tree()};
            });
        }));
         // Populate values
        expect(trav.desc->computed_attrs_offset);
        auto f = trav.desc->computed_attrs()->f;
        for (auto& [key, value] : object) {
            auto ref = f(*trav.address, key);
            if (!ref) raise_AttrNotFound(trav.desc, key);

            ToTreeTraversal<ComputedAttrTraversal> child;
            child.dest = &value;
            trav_computed_attr<visit>(
                child, trav, ref, f, key, AccessMode::Read
            );
        }
        new (trav.dest) Tree(move(object));
    }

    NOINLINE static
    void use_elems (
        const ToTreeTraversal<>& trav, const ElemsDcrPrivate* elems
    ) {
        auto len = elems->chop_flag(AttrFlags::Invisible);
        auto array = UniqueArray<Tree>(Capacity(len));
        for (uint i = 0; i < len; i++) {
            auto acr = elems->elem(i)->acr();
            ToTreeTraversal<ElemTraversal> child;
            child.dest = &array.emplace_back_expect_capacity(Tree());
            trav_elem<visit>(
                child, trav, acr, i, AccessMode::Read
            );
            child.dest->flags |= child.acr->tree_flags();
        }
        new (trav.dest) Tree(move(array));
    }

    static usize read_length (
        const ToTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        usize len;
        length_acr->read(*trav.address,
            AccessCB(len, [](usize& len, AnyPtr v, bool)
        {
            require_readable_length(v.type);
            len = reinterpret_cast<const usize&>(*v.address);
        }));
        return len;
    }

    NOINLINE static
    void use_computed_elems (
        const ToTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        usize len = read_length(trav, length_acr);
        auto array = UniqueArray<Tree>(len);
        expect(trav.desc->computed_elems_offset);
        auto f = trav.desc->computed_elems()->f;
        for (usize i = 0; i < array.size(); i++) {
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.desc, i);
            ToTreeTraversal<ComputedElemTraversal> child;
            child.dest = &array[i];
            trav_computed_elem<visit>(
                child, trav, ref, f, i, AccessMode::Read
            );
        }
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_contiguous_elems (
        const ToTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        usize len = read_length(trav, length_acr);
        auto array = UniqueArray<Tree>(len);
         // If len is 0, don't even bother calling the contiguous_elems
         // function.  This shortcut isn't needed for computed_elems.
        if (array) {
            expect(trav.desc->contiguous_elems_offset);
            auto f = trav.desc->contiguous_elems()->f;
            auto ptr = f(*trav.address);
            for (usize i = 0; i < array.size(); i++) {
                ToTreeTraversal<ContiguousElemTraversal> child;
                child.dest = &array[i];
                trav_contiguous_elem<visit>(
                    child, trav, ptr, f, i, AccessMode::Read
                );
                ptr.address = (Mu*)((char*)child.address + child.desc->cpp_size);
            }
        }
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_delegate (const ToTreeTraversal<>& trav, const Accessor* acr) {
        ToTreeTraversal<DelegateTraversal> child;
        child.dest = trav.dest;
        trav_delegate<visit>(child, trav, acr, AccessMode::Read);
        child.dest->flags |= child.acr->tree_flags();
    }

///// ERRORS

    [[noreturn, gnu::cold]] NOINLINE static
    void fail (const ToTreeTraversal<>& trav) {
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

     // NOINLINE this so its stack requirements don't get applied to visit()
    [[gnu::cold]] NOINLINE static
    bool wrap_exception (const ToTreeTraversal<>& trav) {
        if (diagnostic_serialization) {
            new (trav.dest) Tree(std::current_exception());
            return true;
        }
        else return false;
    }
};

} using namespace in;

Tree item_to_tree (const AnyRef& item, LocationRef loc) {
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
