#include "to-tree.h"

#include "../reflection/description.private.h"
#include "compound.private.h"
#include "scan.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

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
    void start (Tree& r, const AnyRef& item, RouteRef rt, ToTreeOptions opts) {
        plog("to_tree start");
        CurrentBase curb (rt, item);
        KeepRouteCache klc;
        ToTreeTraversal<StartTraversal> child;
        child.dest = &r;
        child.embed_errors = !!(opts & TTO::EmbedErrors);
        trav_start<visit, false>(
            child, item, rt, AccessMode::Read
        );
        plog("to_tree end");
    }

///// PICK STRATEGY

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const ToTreeTraversal<>&>(tr);
        if (trav.embed_errors) {
            visit_embedding_errors(trav);
        }
         // The majority of items are [[likely]] to be atomic.
        else if (auto to_tree = trav.desc->to_tree()) [[likely]] {
            use_to_tree(trav, to_tree->f);
        }
        else if (auto values = trav.desc->values()) {
            use_values(trav, values);
        }
        else no_value_match(trav);
    }

    NOINLINE static
    void visit_embedding_errors (const ToTreeTraversal<>& trav) try {
        if (auto to_tree = trav.desc->to_tree()) [[likely]] {
            use_to_tree(trav, to_tree->f);
        }
        else if (auto values = trav.desc->values()) {
            use_values(trav, values);
        }
        else no_value_match(trav);
    }
    catch (...) { wrap_exception(trav); }

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
        for (u32 i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (values->compare.generic(*trav.address, *value->get_value())) {
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
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (!!(attr->acr()->attr_flags & AttrFlags::Invisible)) continue;

            ToTreeTraversal<AttrTraversal> child;

            child.dest = &object.emplace_back_expect_capacity(
                attr->key, Tree()
            ).second;
            child.embed_errors = trav.embed_errors;
            trav_attr<visit, false>(
                child, trav, attr->acr(), attr->key, AccessMode::Read
            );
            child.dest->flags |= child.acr->tree_flags();
        }
         // Then if there are included or collapsed attrs, rebuild the object
         // while flattening them.
        if (!!(trav.desc->flags & DescFlags::AttrsNeedRebuild)) {
             // Determine length for preallocation
            u32 len = object.size();
            for (u32 i = 0; i < attrs->n_attrs; i++) {
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
            for (u32 i = 0; i < attrs->n_attrs; i++) {
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
                    continue;
                }
                else if (!!(flags & AttrFlags::CollapseOptional)) {
                    if (value.form != Form::Array || (value.meta >> 1) > 1) {
                        raise(e_General,
                            "Attribute with collapse_optional did not "
                            "serialize to an array of 0 or 1 elements"
                        );
                    }
                    if (auto a = AnyArray<Tree>(move(value))) {
                        new (&value) Tree(move(a[0]));
                        SharableBuffer<Tree>::deallocate(a.impl.data);
                        a.impl = {};
                    }
                    else continue; // Drop the attr
                }
                else if (const Tree* def = attr->default_value()) {
                    if (value == *def) continue; // Drop the attr
                }
                new_object.emplace_back_expect_capacity(
                    move(key), move(value)
                );
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
            object = UniqueArray<TreePair>(keys.size(), [&](u32 i){
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
            child.embed_errors = trav.embed_errors;
            trav_computed_attr<visit, false>(
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
        for (u32 i = 0; i < len; i++) {
            auto acr = elems->elem(i)->acr();
            ToTreeTraversal<ElemTraversal> child;
            child.dest = &array.emplace_back_expect_capacity(Tree());
            child.embed_errors = trav.embed_errors;
            trav_elem<visit, false>(
                child, trav, acr, i, AccessMode::Read
            );
            child.dest->flags |= child.acr->tree_flags();
        }
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_computed_elems (
        const ToTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        u32 len;
        read_length_acr(len, AnyPtr(trav.desc, trav.address), length_acr);
        auto array = UniqueArray<Tree>(len);
        expect(trav.desc->computed_elems_offset);
        auto f = trav.desc->computed_elems()->f;
        for (u32 i = 0; i < array.size(); i++) {
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.desc, i);
            ToTreeTraversal<ComputedElemTraversal> child;
            child.dest = &array[i];
            child.embed_errors = trav.embed_errors;
            trav_computed_elem<visit, false>(
                child, trav, ref, f, i, AccessMode::Read
            );
        }
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_contiguous_elems (
        const ToTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        u32 len;
        read_length_acr(len, AnyPtr(trav.desc, trav.address), length_acr);
        auto array = UniqueArray<Tree>(len);
         // If len is 0, don't even bother calling the contiguous_elems
         // function.  This shortcut isn't needed for computed_elems.
        if (array) {
            expect(trav.desc->contiguous_elems_offset);
            auto f = trav.desc->contiguous_elems()->f;
            auto ptr = f(*trav.address);
            for (u32 i = 0; i < array.size(); i++) {
                ToTreeTraversal<ContiguousElemTraversal> child;
                child.dest = &array[i];
                child.embed_errors = trav.embed_errors;
                trav_contiguous_elem<visit, false>(
                    child, trav, ptr, f, i, AccessMode::Read
                );
                ptr.address = (Mu*)(
                    (char*)child.address + Type(child.desc).cpp_size()
                );
            }
        }
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_delegate (const ToTreeTraversal<>& trav, const Accessor* acr) {
        ToTreeTraversal<DelegateTraversal> child;
        child.dest = trav.dest;
        child.embed_errors = trav.embed_errors;
        trav_delegate<visit, false>(child, trav, acr, AccessMode::Read);
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

    NOINLINE static
    void wrap_exception (const ToTreeTraversal<>& trav) {
        expect(trav.embed_errors);
        new (trav.dest) Tree(std::current_exception());
    }
};

} using namespace in;

Tree item_to_tree (const AnyRef& item, RouteRef rt, ToTreeOptions opts) {
    Tree r;
    TraverseToTree::start(r, item, rt, opts);
    return r;
}

UniqueString item_to_string (
    const AnyRef& item, PrintOptions popts, RouteRef rt, ToTreeOptions ttopts
) {
    Tree t;
    TraverseToTree::start(t, item, rt, ttopts);
    return tree_to_string(t, popts);
}

UniqueString show (
    const AnyRef& item, PrintOptions popts, RouteRef rt, ToTreeOptions ttopts
) noexcept {
    return item_to_string(item, popts, rt, ttopts | TTO::EmbedErrors);
}

} using namespace ayu;
