#include "to-tree.h"

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
        child.embed_errors = opts % TTO::EmbedErrors;
        trav_start<visit>(child, item, AC::Read);
        plog("to_tree end");
    }

///// PICK STRATEGY

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const ToTreeTraversal<>&>(tr);
        auto desc = trav.desc();
        if (trav.embed_errors) {
            visit_embedding_errors(trav);
        }
         // The majority of items are [[likely]] to be atomic.
        else if (auto to_tree = desc->to_tree()) [[likely]] {
            use_to_tree(trav, to_tree->f);
        }
        else if (auto values = desc->values()) {
            use_values(trav, values);
        }
        else no_value_match(trav, desc);
    }

    NOINLINE static
    void visit_embedding_errors (const ToTreeTraversal<>& trav) try {
        auto desc = trav.desc();
        if (auto to_tree = desc->to_tree()) [[likely]] {
            use_to_tree(trav, to_tree->f);
        }
        else if (auto values = desc->values()) {
            use_values(trav, values);
        }
        else no_value_match(trav, desc);
    }
    catch (...) { wrap_exception(trav); }

    NOINLINE static
    void no_value_match (
        const ToTreeTraversal<>& trav, const DescriptionPrivate* desc
    ) {
        if (desc->preference() == DescFlags::PreferObject) {
            if (auto keys = desc->keys_acr()) {
                return use_computed_attrs(trav, keys);
            }
            else if (auto attrs = desc->attrs()) {
                if (desc->flags % DescFlags::AttrsNeedRebuild) {
                    return use_attrs(trav, attrs);
                }
                else if (attrs->n_attrs) {
                    return use_attrs_no_rebuild(trav, attrs);
                }
                else {
                    new (trav.dest) Tree(AnyArray<TreePair>());
                    return;
                }
            }
            else never();
        }
        else if (desc->preference() == DescFlags::PreferArray) {
            if (auto length = desc->length_acr()) {
                if (desc->flags % DescFlags::ElemsContiguous) {
                    return use_contiguous_elems(trav, length);
                }
                else {
                    return use_computed_elems(trav, length);
                }
            }
            else if (auto elems = desc->elems()) {
                if (desc->flags % DescFlags::ElemsNeedRebuild) {
                    return use_elems_collapse(trav, elems);
                }
                else if (elems->n_elems) {
                    return use_elems(trav, elems);
                }
                else {
                    new (trav.dest) Tree(AnyArray<Tree>());
                    return;
                }
            }
            else never();
        }
        else if (auto acr = desc->delegate_acr()) {
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
        no_value_match(trav, trav.desc());
    }

    NOINLINE static
    void use_attrs_no_rebuild (
        const ToTreeTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
        expect(attrs->n_attrs);
        auto object = UniqueArray<TreePair>(Capacity(attrs->n_attrs));
        expect(attrs->n_attrs);
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (attr->acr()->attr_flags % AttrFlags::Invisible) continue;

            ToTreeTraversal<AttrTraversal> child;

            child.dest = &object.emplace_back_expect_capacity(
                attr->key, Tree()
            ).second;
            child.embed_errors = trav.embed_errors;
            trav_attr<visit>(
                child, trav, attr->acr(), attr->key, AC::Read
            );
            child.dest->flags |= child.acr->tree_flags;
        }
        new (trav.dest) Tree(move(object));
    }

    NOINLINE static
    void use_attrs (
        const ToTreeTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
        expect(attrs->n_attrs);
        auto object = UniqueArray<TreePair>(Capacity(attrs->n_attrs));
         // First just build the object as though none of the attrs are
         // collapsed, then rebuild the object while collapsing attrs.
        expect(attrs->n_attrs);
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (attr->acr()->attr_flags % AttrFlags::Invisible) continue;

            ToTreeTraversal<AttrTraversal> child;

            child.dest = &object.emplace_back_expect_capacity(
                attr->key, Tree()
            ).second;
            child.embed_errors = trav.embed_errors;
            trav_attr<visit>(
                child, trav, attr->acr(), attr->key, AC::Read
            );
            child.dest->flags |= child.acr->tree_flags;
        }
         // Determine length for preallocation
        u32 len = object.size();
        expect(attrs->n_attrs);
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto flags = attrs->attr(i)->acr()->attr_flags;
             // Ignore HasDefault; it can only decrease the length by 1, and
             // checking whether it does requires comparing Trees, so I'd
             // rather just overallocate.
            if (flags % (AttrFlags::Collapse|AttrFlags::CollapseOptional)) {
                 // This coincidentally works for both of these flags.
                len = len + object[i].second.size - 1;
            }
        }
         // Allocate
        auto new_object = decltype(object)(Capacity(len));
         // Selectively flatten
        expect(attrs->n_attrs);
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto flags = attr->acr()->attr_flags;
            auto key = move(object[i].first);
            Tree value = move(object[i].second);
            if (flags % AttrFlags::Collapse) {
                if (value.form != Form::Object) {
                    raise(e_General,
                        "Collapsed item did not serialize to an object"
                    );
                }
                 // DON'T consume sub object because it could be shared.
                for (auto& pair : AnyArray<TreePair>(move(value))) {
                    new_object.emplace_back_expect_capacity(pair);
                }
                continue;
            }
            else if (flags % AttrFlags::CollapseOptional) {
                if (value.form != Form::Array || value.size > 1) {
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
            expect(!pair.second.size);
        }
#endif
        SharableBuffer<TreePair>::deallocate(object.impl.data);
        object.impl = {};
         // This will check for duplicates in debug mode.
        new (trav.dest) Tree(move(new_object));
    }

    NOINLINE static
    void use_computed_attrs (
        const ToTreeTraversal<>& trav, const Accessor* keys_acr
    ) {
         // Populate keys
        UniqueArray<TreePair> object;
        keys_acr->read(*trav.address,
            AccessCB(object, [](auto& object, Type t, Mu* v)
        {
            auto& ks = require_readable_keys(t, v);
            expect(!object.owned());
            object = UniqueArray<TreePair>(ks.size(), [&](u32 i){
                return TreePair{ks[i], Tree()};
            });
        }));
         // Populate values
        for (auto& [key, value] : object) {
            auto f = expect(trav.desc()->computed_attrs())->f;
            auto ref = f(*trav.address, key);
            if (!ref) raise_AttrNotFound(trav.type, key);

            ToTreeTraversal<ComputedAttrTraversal> child;
            child.dest = &value;
            child.embed_errors = trav.embed_errors;
            trav_computed_attr<visit>(
                child, trav, ref, f, key, AC::Read
            );
        }
        new (trav.dest) Tree(move(object));
        expect(!object.owned());
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
            trav_elem<visit>(
                child, trav, acr, i, AC::Read
            );
            child.dest->flags |= child.acr->tree_flags;
        }
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_elems_collapse (
        const ToTreeTraversal<>& trav, const ElemsDcrPrivate* elems
    ) {
        expect(elems->n_elems);
        auto array = UniqueArray<Tree>(Capacity(elems->n_elems));
        expect(elems->n_elems);
        for (u32 i = 0; i < elems->n_elems; i++) {
            auto acr = elems->elem(i)->acr();
            ToTreeTraversal<ElemTraversal> child;
            child.dest = &array.emplace_back_expect_capacity(Tree());
            child.embed_errors = trav.embed_errors;
            trav_elem<visit>(
                child, trav, acr, i, AC::Read
            );
            child.dest->flags |= child.acr->tree_flags;
        }
        expect(array.size() >= 1);
        Tree collapsed = move(array.back());
        array.pop_back();
        if (collapsed.form != Form::Array) {
            raise(e_General, "Collapsed elem did not serialize to an Array tree.");
        }
        array.append(AnyArray<Tree>(move(collapsed)));
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_computed_elems (
        const ToTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        u32 len;
        read_length_acr(len, trav.type, trav.address, length_acr);
        auto array = UniqueArray<Tree>(Capacity(len));
        for (u32 i = 0; i < len; i++) {
            auto f = expect(trav.desc()->computed_elems())->f;
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.type, i);
            ToTreeTraversal<ComputedElemTraversal> child;
            child.dest = &array.emplace_back_expect_capacity(Tree());
            child.embed_errors = trav.embed_errors;
            trav_computed_elem<visit>(
                child, trav, ref, f, i, AC::Read
            );
        }
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_contiguous_elems (
        const ToTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        u32 len;
        read_length_acr(len, trav.type, trav.address, length_acr);
         // If len is 0, don't even bother calling the contiguous_elems
         // function.  This shortcut isn't needed for computed_elems.
        if (!len) {
            new (trav.dest) Tree(AnyArray<Tree>());
            return;
        }
        auto array = UniqueArray<Tree>(Capacity(len));
        auto f = expect(trav.desc()->contiguous_elems())->f;
        auto ptr = f(*trav.address);
        expect(len);
        for (u32 i = 0; i < len; i++) {
            ToTreeTraversal<ContiguousElemTraversal> child;
            child.dest = &array.emplace_back_expect_capacity(Tree());
            child.embed_errors = trav.embed_errors;
            trav_contiguous_elem<visit>(
                child, trav, ptr, f, i, AC::Read
            );
            ptr.address = (Mu*)(
                (char*)child.address + child.type.cpp_size()
            );
        }
        new (trav.dest) Tree(move(array));
    }

    NOINLINE static
    void use_delegate (const ToTreeTraversal<>& trav, const Accessor* acr) {
        ToTreeTraversal<DelegateTraversal> child;
        child.dest = trav.dest;
        child.embed_errors = trav.embed_errors;
        trav_delegate<visit>(child, trav, acr, AC::Read);
        child.dest->flags |= child.acr->tree_flags;
    }

///// ERRORS

    [[noreturn, gnu::cold]] NOINLINE static
    void fail (const ToTreeTraversal<>& trav) {
        auto desc = trav.desc();
        if (desc->values()) {
            raise(e_ToTreeValueNotFound, cat(
                "No value for type ", trav.type.name(),
                " matches the item's value"
            ));
        }
        else raise(e_ToTreeNotSupported, cat(
            "Item of type ", trav.type.name(), " does not support to_tree"
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
