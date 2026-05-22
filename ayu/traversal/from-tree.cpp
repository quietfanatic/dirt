#include "from-tree.h"
#include <memory>
#include "../reflection/description.private.h"
#include "../resources/resource.h"
#include "compound.private.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

struct SwizzleOp {
    SharedRoute base;
    Link item;
    SwizzleFunc<Mu>* f;
    Tree tree;
};
struct InitOp {
    SharedRoute base;
    Link item;
    InitFunc<Mu>* f;
    double order;
};
} }
template <>
struct uni::IsTriviallyRelocatableS<ayu::in::SwizzleOp> {
    static constexpr bool value = true;
};
template <>
struct uni::IsTriviallyRelocatableS<ayu::in::InitOp> {
    static constexpr bool value = true;
};
namespace ayu { namespace in {

struct IFTContext {
    static IFTContext* current;
    IFTContext* previous;
    IFTContext () : previous(current) {
        current = this;
    }
    ~IFTContext () {
        assume(current == this);
        current = previous;
    }

    UniqueArray<SwizzleOp> swizzle_ops;
    UniqueArray<InitOp> init_ops;
};

IFTContext* IFTContext::current = null;

struct FromTreeTraversalHead {
    const Tree* tree;
};
struct ClaimAttrsTraversalHead {
    u32* next_list;
};
template <class T = Traversal>
struct FromTreeTraversal : FromTreeTraversalHead, T { };
template <class T = Traversal>
struct ClaimAttrsTraversal :
    ClaimAttrsTraversalHead, FromTreeTraversal<T>
{ };

struct TraverseFromTree {

///// START, DOING SWIZZLE AND INIT

    static
    void start (
        const Link& item, const Tree& tree, RouteRef rt,
        FromTreeOptions opts
    ) {
        plog("from_tree start");
        if (tree.form == Form::Undefined) {
            raise(e_FromTreeFormRejected,
                "Undefined tree given to item_from_tree"
            );
        }
        if (opts % FTO::DelaySwizzle && IFTContext::current) {
             // Delay swizzle and inits to the outer item_from_tree call.  Basically
             // this just means keep the current context instead of making a new one.
            if (!rt) raise(e_General, "Cannot call item_from_tree with FTO::DelaySwizzle without an explicit route parameter.");
            start_without_context(item, tree, rt);
        }
        else start_with_context(item, tree, rt);
        plog("from_tree end");
    }

    NOINLINE static
    void start_with_context (
        const Link& item, const Tree& tree, RouteRef rt
    ) {
         // Start a resource transaction so that dependency loads are all or
         // nothing.
        ResourceTransaction tr;
        IFTContext ctx;
        start_without_context(item, tree, rt);
        if (!!(ctx.swizzle_ops) | !!(ctx.init_ops)) {
            do_swizzle_init(ctx);
        }
        assume(!ctx.swizzle_ops.owned());
        assume(!ctx.init_ops.owned());
    }

    NOINLINE static
    void start_without_context (
        const Link& item, const Tree& tree, RouteRef rt
    ) {
        CurrentBase curb (rt, item);
        FromTreeTraversal<StartTraversal> child;
        child.tree = &tree;
        trav_start<visit>(child, item, AC::Write);
    }

    NOINLINE static
    void do_swizzle_init (IFTContext& ctx) {
        if (ctx.swizzle_ops) {
            ctx.swizzle_ops.consume([](SwizzleOp&& op){
                assume(!op.base->parent());
                CurrentBase curb (move(op.base));
                try {
                    op.item.modify(AccessCB(op, [](auto& op, Type, Mu* v){
                        op.f(*v, op.tree);
                    }));
                }
                catch (...) { tag_error_with_item(op.item); }
                assume(!op.base);
            });
             // Swizzling might add more swizzle ops; this will happen if we're
             // swizzling a pointer which points to a separate resource; that
             // resource will be load()ed in op.f().
            do_swizzle_init(ctx);
        }
        else if (ctx.init_ops) {
            ctx.init_ops.consume([](InitOp&& op){
                assume(!op.base->parent());
                CurrentBase curb (move(op.base));
                try {
                    op.item.modify(AccessCB(op, [](auto& op, Type, Mu* v){
                        op.f(*v);
                    }));
                }
                catch (...) { tag_error_with_item(op.item); }
                assume(!op.base);
            });
             // Initting might add more swizzle or init ops.  It'd be weird, but
             // it's allowed for an init() to load another resource.
            do_swizzle_init(ctx);
        }
    }

///// PICK STRATEGY

    static // not noinline
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const FromTreeTraversal<>&>(tr);
        auto desc = trav.desc();
        if (auto from_tree = desc->from_tree()) {
            use_from_tree(trav, from_tree->f);
        }
        else after_from_tree(trav, desc);
    }

    NOINLINE static
    void after_from_tree (
        const FromTreeTraversal<>& trav, const DescriptionPrivate* desc
    ) {
        if (auto values = desc->values()) {
            if (desc->flags % DescFlags::ValuesAllStrings) {
                if (trav.tree->form == Form::String) {
                    use_values_all_strings(trav, values);
                }
                else no_match(trav, desc);
            }
            else use_values(trav, values);
        }
        else no_match(trav, desc);
    }

    NOINLINE static
    void no_match (
        const FromTreeTraversal<>& trav, const DescriptionPrivate* desc
    ) {
         // Now the behavior depends on what form of tree we got
        if (trav.tree->form == Form::Object) {
            if (auto keys = desc->keys_acr()) {
                return use_computed_attrs(trav, keys);
            }
            else if (auto attrs = desc->attrs()) {
                return use_attrs(trav, attrs);
            }
             // fallthrough
        }
        else if (trav.tree->form == Form::Array) {
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
                else return use_elems(trav, elems);
            }
             // fallthrough
        }
         // Nothing matched, so try delegate
        if (auto acr = desc->delegate_acr()) {
            use_delegate(trav, acr);
        }
         // Still nothing?  Allow swizzle with no from_tree.
        else if (desc->swizzle_offset) {
            register_swizzle_init(trav, desc);
        }
        else fail(trav);
    }

///// FROM TREE STRATEGY

    NOINLINE static
    void use_from_tree (
        const FromTreeTraversal<>& trav, FromTreeFunc<Mu>* f
    ) {
        if (f(*trav.address, *trav.tree)) {
            finish_item(trav);
        }
        else after_from_tree(trav, trav.desc());
    }

///// OBJECT STRATEGIES

    NOINLINE static
    void use_attrs (
        const FromTreeTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
         // Build a linked list of indexes so that we can claim attrs in
         // constant time.  next_list = next_list_buffer + 1, so that:
         //   - next_list[-1] is the index of the first non-claimed attr
         //     (its initial value is 0 for the first attr).
         //   - next_list[i] is the index of whatever non-claimed attr is
         //     next after the ith attr.
         //   - If next_list[i] is -1, that means i is the index of the last
         //     non-claimed attr.
         // When an attr is claimed, its link will be deleted from the
         // linked list by setting next_list[previous index] to
         // next_list[i].
         //
         // This makes the attr-claiming algorithm O(n^2) in the worst case
         // instead of O(n^3).  It also makes the best case O(n) (when all attrs
         // of the item are provided and in the same order they're declared in
         // the AYU_DESCRIBE block).
         //
         // In theory, we could make the worst-case O(n) as well by stuffing the
         // keys in an unordered_map or something, but the extra overhead is
         // unlikely to be worth it.
         //
         // (Note that using -1 as a sentinel does not reduce the usable array
         // size by 1.  The maximum array tree size is u32(-1), for which
         // u32(-1) is not a valid index.)
        auto len = trav.tree->size;
        auto next_list_buf = std::unique_ptr<u32[]>(new u32[len+1]);
        for (u32 i = 0; i < len; i++) next_list_buf[i] = i;
        next_list_buf[len] = u32(-1);

        claim_attrs_use_attrs(trav, &next_list_buf[0] + 1, attrs);
        if (next_list_buf[0] != u32(-1)) {
            assume(trav.tree->form == Form::Object);
            raise_AttrRejected(
                trav.type, trav.tree->data.as_object_ptr[next_list_buf[0]].first
            );
        }
    }

    NOINLINE static
    void use_computed_attrs (
        const FromTreeTraversal<>& trav, const Accessor* keys_acr
    ) {
         // Computed attrs always take the entire object, so we don't need to
         // allocate a next_list.
        assume(trav.tree->form == Form::Object);
        set_keys(trav, Slice<TreePair>(*trav.tree), keys_acr);
        auto desc = trav.desc();
        assume(desc->computed_attrs_offset);
        auto f = desc->computed_attrs()->f;
        assume(trav.tree->form == Form::Object);
        for (auto& pair : Slice<TreePair>(*trav.tree)) {
            write_computed_attr(trav, pair, f);
        }
        finish_item(trav);
    }

    static // inlinable
    void claim_attrs (const Traversal& tr) {
        auto& trav = static_cast<const ClaimAttrsTraversal<>&>(tr);
        auto desc = trav.desc();
        if (auto keys = desc->keys_acr()) {
            claim_attrs_use_computed_attrs(trav, trav.next_list, keys);
        }
        else if (auto attrs = desc->attrs()) {
            claim_attrs_use_attrs(trav, trav.next_list, attrs);
        }
        else if (auto acr = desc->delegate_acr()) {
            claim_attrs_use_delegate(trav, trav.next_list, acr);
        }
        else raise_AttrsNotSupported(trav.type);
    }

    NOINLINE static
    void claim_attrs_use_attrs (
        const FromTreeTraversal<>& trav, u32* next_list,
        const AttrsDcrPrivate* attrs
    ) {
        assume(trav.tree->form == Form::Object);
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto flags = attr->acr()->attr_flags;
             // First try matching attr directly even if it's collapsible
            u32* prev_next; u32 j;
            for (
                prev_next = &next_list[-1], j = *prev_next;
                j != u32(-1);
                prev_next = &next_list[j], j = *prev_next
            ) {
                auto& [key, value] = trav.tree->data.as_object_ptr[j];
                if (key == attr->key) {
                    if (!(flags % AttrFlags::Ignored)) {
                        Indestructible<Tree> singleton;
                        FromTreeTraversal<AttrTraversal> child;
                        if (flags % AttrFlags::CollapseOptional) {
                            new (&*singleton) Tree(
                                StaticArray<Tree>(&value, 1)
                            );
                            child.tree = &*singleton;
                        }
                        else child.tree = &value;
                        trav_attr<visit>(
                            child, trav, attr->acr(), attr->key, AC::Write
                        );
                    }
                     // Claim attr by deleting link
                    *prev_next = next_list[j];
                    goto next_attr;
                }
            }
             // No match, try optional, collapsing
            if (flags % AttrFlags::Collapse) {
                 // Recurse with the same tree.
                ClaimAttrsTraversal<AttrTraversal> child;
                child.next_list = next_list;
                child.tree = trav.tree;
                trav_attr<claim_attrs>(
                    child, trav, attr->acr(), attr->key, AC::Write
                );
            }
            else if (flags % (AttrFlags::Optional|AttrFlags::Ignored)) {
                 // Leave the attribute in its default-constructed state.
            }
            else {
                Indestructible<Tree> empty;
                FromTreeTraversal<AttrTraversal> child;
                if (flags % AttrFlags::CollapseOptional) {
                     // If the attribute was not provided and has
                     // collapse_optional set, deserialize the item with an
                     // empty array.
                    new (&*empty) Tree(SharedArray<Tree>());
                    child.tree = &*empty;
                }
                else if (const Tree* def = attr->default_value()) {
                    child.tree = def;
                }
                else raise_AttrMissing(trav.type, attr->key);
                trav_attr<visit>(
                    child, trav, attr->acr(), attr->key, AC::Write
                );
            }
            next_attr:;
        }
         // The claim_* stack doesn't call finish_item so call it here.
        finish_item(trav);
    }

    NOINLINE static
    void claim_attrs_use_computed_attrs (
        const FromTreeTraversal<>& trav, u32* next_list,
        const Accessor* keys_acr
    ) {
         // We should only get here if a parent item collapsed a child item that
         // has computed attrs.
        assume(trav.tree->form == Form::Object);
        set_keys(trav, Slice<TreePair>(*trav.tree), keys_acr);
        auto f = assume(trav.desc()->computed_attrs())->f;
        u32* prev_next; u32 i;
        for (
            prev_next = &next_list[-1], i = *prev_next;
            i != u32(-1);
            prev_next = &next_list[i], i = *prev_next
        ) {
            assume(trav.tree->form == Form::Object);
            write_computed_attr(trav, trav.tree->data.as_object_ptr[i], f);
        }
         // Consume entire list
        next_list[-1] = u32(-1);
        finish_item(trav);
    }

    NOINLINE static
    void claim_attrs_use_delegate (
        const FromTreeTraversal<>& trav, u32* next_list,
        const Accessor* acr
    ) {
        ClaimAttrsTraversal<DelegateTraversal> child;
        child.next_list = next_list;
        child.tree = trav.tree;
        trav_delegate<claim_attrs>(child, trav, acr, AC::Write);
    }

    static
    void set_keys (
        const FromTreeTraversal<>& trav, Slice<TreePair> object,
        const Accessor* keys_acr
    ) {
        if (keys_acr->caps % AC::Write) {
            set_keys_write(trav, object, keys_acr);
        }
        else {
            set_keys_readonly(trav, object, keys_acr);
        }
    }

    NOINLINE static
    void set_keys_write (
        const FromTreeTraversal<>& trav, Slice<TreePair> object,
        const Accessor* keys_acr
    ) {
         // Writable keys, so write them.
        auto keys = UniqueArray<SharedString>(
            object.size(), [&object](u32 i){ return object[i].first; }
        );
        keys_acr->write(*trav.address,
            AccessCB(move(keys), [](auto&& keys, Type t, Mu* v)
        {
            auto& ks = require_writeable_keys(t, v);
            ks = move(keys);
        }));
        assume(!keys.owned());
    }

    NOINLINE static
    void set_keys_readonly (
        const FromTreeTraversal<>& trav, Slice<TreePair> object,
        const Accessor* keys_acr
    ) {
         // Readonly keys?  Read them and check that they match.
        SharedArray<SharedString> keys;
        keys_acr->read(*trav.address,
            AccessCB(keys, [](auto& keys, Type t, Mu* v)
        {
            auto& ks = require_readable_keys(t, v);
            new (&keys) SharedArray<SharedString>(ks);
        }));
#ifndef NDEBUG
         // Check returned keys for duplicates
        for (u32 i = 0; i < keys.size(); i++)
        for (u32 j = 0; j < i; j++) {
            assume(keys[i] != keys[j]);
        }
#endif
        if (keys.size() >= object.size()) {
            for (auto& required : keys) {
                for (auto& given : object) {
                    if (given.first == required) goto next_required;
                }
                raise_AttrMissing(trav.type, required);
                next_required:;
            }
        }
        else [[unlikely]] {
             // Too many keys given
            for (auto& given : object) {
                for (auto& required : keys) {
                    if (required == given.first) goto next_given;
                }
                raise_AttrRejected(trav.type, given.first);
                next_given:;
            }
            never();
        }
    }

    static
    void write_computed_attr (
        const FromTreeTraversal<>& trav, const TreePair& pair, AttrFunc<Mu>* f
    ) {
        auto& [key, value] = pair;
        Link ref = f(*trav.address, key);
        if (!ref) raise_AttrNotFound(trav.type, key);
        FromTreeTraversal<ComputedAttrTraversal> child;
        child.tree = &value;
        trav_computed_attr<visit>(child, trav, ref, f, key, AC::Write);
    }

///// ARRAY STRATEGIES

    NOINLINE static
    void use_elems (
        const FromTreeTraversal<>& trav, const ElemsDcrPrivate* elems
    ) {
         // Check whether length is acceptable
        u32 min = elems->chop_flag(AttrFlags::Optional);
        assume(trav.tree->form == Form::Array);
        auto array = Slice<Tree>(*trav.tree);
        if (array.size() < min || array.size() > elems->n_elems) {
            raise_LengthRejected(trav.type, min, elems->n_elems, array.size());
        }
        for (u32 i = 0; i < array.size(); i++) {
            auto acr = elems->elem(i)->acr();
            if (acr->attr_flags % AttrFlags::Ignored) continue;
            FromTreeTraversal<ElemTraversal> child;
            child.tree = &array[i];
            trav_elem<visit>(child, trav, acr, i, AC::Write);
        }
        finish_item(trav);
    }

    NOINLINE static
    void use_elems_collapse (
        const FromTreeTraversal<>& trav, const ElemsDcrPrivate* elems
    ) {
         // We can only check the lower bound right now.  The upper bound will
         // be checked by the collapsed child item.
        u32 collapsed_i = elems->n_elems - 1;
        assume(trav.tree->form == Form::Array);
        auto array = Slice<Tree>(*trav.tree);
        if (array.size() < collapsed_i) {
            raise_LengthRejected(trav.type, collapsed_i, u32(-1), array.size());
        }
        for (u32 i = 0; i < collapsed_i; i++) {
            auto acr = elems->elem(i)->acr();
            assume(!(acr->attr_flags % AttrFlags::Ignored));
            FromTreeTraversal<ElemTraversal> child;
            child.tree = &array[i];
            trav_elem<visit>(child, trav, acr, i, AC::Write);
        }
        auto acr = elems->elem(collapsed_i)->acr();
        assume(acr->attr_flags % AttrFlags::Collapse);
        FromTreeTraversal<ElemTraversal> child;
        Tree collapsed = Tree(SharedArray(array.slice(collapsed_i)));
        child.tree = &collapsed;
        trav_elem<visit>(child, trav, acr, collapsed_i, AC::Write);

        finish_item(trav);
    }

    NOINLINE static
    void use_computed_elems (
        const FromTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        assume(trav.tree->form == Form::Array);
        auto array = Slice<Tree>(*trav.tree);
        u32 len = array.size();
        write_length_acr(len, trav.type, trav.address, length_acr);
        auto f = assume(trav.desc()->computed_elems())->f;
        for (u32 i = 0; i < array.size(); i++) {
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.type, i);
            FromTreeTraversal<ComputedElemTraversal> child;
            child.tree = &array[i];
            trav_computed_elem<visit>(
                child, trav, ref, f, i, AC::Write
            );
        }
        finish_item(trav);
    }

    NOINLINE static
    void use_contiguous_elems (
        const FromTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        assume(trav.tree->form == Form::Array);
        auto array = Slice<Tree>(*trav.tree);
        u32 len = array.size();
        write_length_acr(len, trav.type, trav.address, length_acr);
        if (array) {
            auto f = assume(trav.desc()->contiguous_elems())->f;
            auto ptr = f(*trav.address);
            for (u32 i = 0; i < array.size(); i++) {
                FromTreeTraversal<ContiguousElemTraversal> child;
                child.tree = &array[i];
                trav_contiguous_elem<visit>(
                    child, trav, ptr, f, i, AC::Write
                );
                ptr.address = (Mu*)((char*)ptr.address + ptr.type().cpp_size());
            }
        }
        finish_item(trav);
    }

///// OTHER STRATEGIES

    NOINLINE static
    void use_values_all_strings (
        const FromTreeTraversal<>& trav, const ValuesDcrPrivate* values
    ) {
        for (u32 i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
             // These are for optimization, not safety
            assume(trav.tree->form == Form::String);
            assume(value->name.form == Form::String);
            if (Str(*trav.tree) == Str(value->name)) {
                values->assign.generic(*trav.address, *value->get_value());
                return finish_item(trav);
            }
        }
        no_match(trav, trav.desc());
    }

    NOINLINE static
    void use_values (
        const FromTreeTraversal<>& trav, const ValuesDcrPrivate* values
    ) {
        for (u32 i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (*trav.tree == value->name) {
                values->assign.generic(*trav.address, *value->get_value());
                return finish_item(trav);
            }
        }
        no_match(trav, trav.desc());
    }

    NOINLINE static
    void use_delegate (
        const FromTreeTraversal<>& trav, const Accessor* acr
    ) {
        FromTreeTraversal<DelegateTraversal> child;
        child.tree = trav.tree;
        trav_delegate<visit>(child, trav, acr, AC::Write);
        finish_item(trav);
    }

///// REGISTERING SWIZZLE AND INIT

    static
    void finish_item (const FromTreeTraversal<>& trav) {
         // Now register swizzle and init ops.  We're doing it now instead of at the
         // beginning to make sure that children get swizzled and initted before
         // their parent.
        auto desc = trav.desc();
        if (!!desc->swizzle_offset | !!desc->init_offset) {
            register_swizzle_init(trav, desc);
        }
         // Done
    }

    NOINLINE static
    void register_swizzle_init (
        const FromTreeTraversal<>& trav,
        const DescriptionPrivate* desc
    ) {
         // We're repeating the to_link work if there's both a swizzle and
         // an init, but almost no types are going to have both.
        if (auto swizzle = desc->swizzle()) {
            auto& op = IFTContext::current->swizzle_ops.emplace_back(
                current_base, Link(), swizzle->f, *trav.tree
            );
            trav.to_link(&op.item);
        }
        if (auto init = desc->init()) {
            auto& init_ops = IFTContext::current->init_ops;
            u32 i;
            for (i = init_ops.size(); i > 0; --i) {
                if (init->order >= init_ops[i-1].order) break;
            }
            auto& op = init_ops.emplace(
                i, current_base, Link(), init->f, init->order
            );
            trav.to_link(&op.item);
        }
    }

///// ERRORS

    [[noreturn, gnu::cold]] NOINLINE static
    void fail (const FromTreeTraversal<>& trav) {
         // If we got here, we failed to find any method to from_tree this item.
         // Go through maybe a little too much effort to figure out what went
         // wrong.
        if (trav.tree->form == Form::Error) {
             // Dunno how a lazy error managed to smuggle itself this far.  Give
             // it the attention it deserves.
            std::rethrow_exception(std::exception_ptr(*trav.tree));
        }
        auto desc = trav.desc();
        bool object_rejected = trav.tree->form == Form::Object &&
            (desc->values() || desc->accepts_array());
        bool array_rejected = trav.tree->form == Form::Array &&
            (desc->values() || desc->accepts_object());
        bool other_rejected =
            desc->accepts_array() || desc->accepts_object();
        if (object_rejected || array_rejected || other_rejected) {
            raise_FromTreeFormRejected(trav.type, trav.tree->form);
        }
        else if (desc->values()) {
            raise(e_FromTreeValueNotFound, cat(
                "No value for type ", trav.type.name(),
                " matches the provided tree ", tree_to_string(*trav.tree)
            ));
        }
        else raise(e_FromTreeNotSupported, cat(
            "Item of type ", trav.type.name(), " does not support from_tree."
        ));
    }
};

} using namespace in;

void item_from_tree (
    const Link& item, const Tree& tree, RouteRef rt,
    FromTreeOptions opts
) {
    TraverseFromTree::start(item, tree, rt, opts);
}

bool currently_running_from_tree () noexcept {
    return IFTContext::current;
}

void raise_FromTreeFormRejected (Type t, Form f) {
    raise(e_FromTreeFormRejected, cat(
        "Item of type ", t.name(),
        " does not support from_tree with a tree of form ", show(&f)
    ));
}

} // ayu
