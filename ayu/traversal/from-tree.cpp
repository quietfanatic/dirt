#include "from-tree.h"

#include <memory>
#include "compound.private.h"

namespace ayu {
namespace in {

struct SwizzleOp {
    SwizzleFunc<Mu>* f;
    Reference item;
     // This can't be TreeRef because the referenced Tree could go away after a
     // nested from_tree is called with DELAY_SWIZZLE
    Tree tree;
    Location loc;

    ALWAYS_INLINE
    SwizzleOp (SwizzleFunc<Mu>* f, Reference&& i, const Tree& t, Location&& l) :
        f(f), item(move(i)), tree(t), loc(move(l))
    { }
     // Allow optimized reallocation
    ALWAYS_INLINE
    SwizzleOp (SwizzleOp&& o) {
        std::memcpy((void*)this, &o, sizeof(SwizzleOp));
        std::memset((void*)&o, 0, sizeof(SwizzleOp));
    }
};
struct InitOp {
    InitFunc<Mu>* f;
    Reference item;
    Location loc;

    ALWAYS_INLINE
    InitOp (InitFunc<Mu>* f, Reference&& i, Location&& l) :
        f(f), item(move(i)), loc(move(l))
    { }
     // Allow optimized reallocation
    ALWAYS_INLINE
    InitOp (InitOp&& o) {
        std::memcpy((void*)this, &o, sizeof(InitOp));
        std::memset((void*)&o, 0, sizeof(InitOp));
    }
};
struct IFTContext {
    static IFTContext* current;
    IFTContext* previous;
    IFTContext () : previous(current) {
        current = this;
    }
    ~IFTContext () {
        expect(current == this);
        current = previous;
    }

    UniqueArray<SwizzleOp> swizzle_ops;
    UniqueArray<InitOp> init_ops;
};

IFTContext* IFTContext::current = null;

struct TraverseFromTree {

///// START, DOING SWIZZLE AND INIT

    static
    void start (
        const Reference& item, const Tree& tree, LocationRef loc,
        ItemFromTreeFlags flags
    ) {
        if (tree.form == Form::Undefined) {
            raise(e_FromTreeFormRejected,
                "Undefined tree given to item_from_tree"
            );
        }
        if (flags & DELAY_SWIZZLE && IFTContext::current) {
             // Delay swizzle and inits to the outer item_from_tree call.  Basically
             // this just means keep the current context instead of making a new one.
            start_without_context(item, tree, loc);
        }
        else start_with_context(item, tree, loc);
    }

    NOINLINE static
    void start_with_context (
        const Reference& item, const Tree& tree, LocationRef loc
    ) {
        IFTContext ctx;
        start_without_context(item, tree, loc);
        do_swizzle_init(ctx);
        expect(!ctx.swizzle_ops.owned());
        expect(!ctx.init_ops.owned());
    }

    NOINLINE static
    void start_without_context (
        const Reference& item, const Tree& tree, LocationRef loc
    ) {
        PushBaseLocation pbl(*loc ? *loc : Location(item));
        Traversal::start(item, loc, false, AccessMode::Write,
            [&tree](const Traversal& trav)
        { traverse(trav, tree); });
    }

    NOINLINE static
    void do_swizzle_init (IFTContext& ctx) {
        if (ctx.swizzle_ops) do_swizzle(ctx);
        else if (ctx.init_ops) do_init(ctx);
    }

    NOINLINE static
    void do_swizzle (IFTContext& ctx) {
        expect(ctx.swizzle_ops);
         // Do an explicit move construct to clear the source array
        for (auto ops = move(ctx.swizzle_ops); auto& op : ops) {
            expect(op.loc);
            PushBaseLocation pbl (op.loc);
             // TODO: wrap error messages
            op.item.access(AccessMode::Modify, [&op](Mu& v){
                op.f(v, op.tree);
            });
        }
         // Swizzling might add more swizzle ops; this will happen if we're
         // swizzling a pointer which points to a separate resource; that
         // resource will be load()ed in op.f().
        do_swizzle_init(ctx);
    }

    NOINLINE static
    void do_init (IFTContext& ctx) {
        expect(ctx.init_ops);
        for (auto ops = move(ctx.init_ops); auto& op : ops) {
            expect(op.loc);
            PushBaseLocation pbl (op.loc);
            op.item.access(AccessMode::Modify, [&op](Mu& v){
                op.f(v);
            });
        }
         // Initting might add more swizzle or init ops.  It'd be weird, but
         // it's allowed for an init() to load another resource.
        do_swizzle_init(ctx);
    }

///// PICK STRATEGY

    NOINLINE static
    void traverse (const Traversal& trav, const Tree& tree) {
         // If description has a from_tree, just use that.
        if (auto from_tree = trav.desc->from_tree()) [[likely]] {
            use_from_tree(trav, tree, from_tree->f);
        }
         // Now the behavior depends on what kind of tree we've been given
        else if (tree.form == Form::Object) {
            if (auto attrs = trav.desc->attrs()) {
                 // TODO: shortcut empty attrs?
                use_attrs(trav, tree, attrs);
            }
            else if (auto keys = trav.desc->keys_acr()) {
                expect(trav.desc->attr_func_offset);
                auto f = trav.desc->attr_func()->f;
                use_computed_attrs(trav, tree, keys, f);
            }
            else no_match(trav, tree);
        }
        else if (tree.form == Form::Array) {
            if (auto elems = trav.desc->elems()) {
                use_elems(trav, tree, elems);
            }
            else if (auto length = trav.desc->length_acr()) {
                expect(trav.desc->elem_func_offset);
                auto f = trav.desc->elem_func()->f;
                use_computed_elems(trav, tree, length, f);
            }
            else no_match(trav, tree);
        }
        else if (auto values = trav.desc->values()) {
             // All other tree types support the values descriptor
            use_values(trav, tree, values);
        }
        else no_match(trav, tree);
    }

    NOINLINE static
    void no_match (
        const Traversal& trav, const Tree& tree
    ) {
         // Nothing matched, so try delegate
        if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(trav, tree, acr);
        }
         // Still nothing?  Allow swizzle with no from_tree.
        else if (trav.desc->swizzle_offset) {
            register_swizzle_init(trav, tree);
        }
        else fail(trav, tree);
    }

///// FROM TREE STRATEGY

    NOINLINE static
    void use_from_tree (
        const Traversal& trav, const Tree& tree, FromTreeFunc<Mu>* f
    ) {
        f(*trav.address, tree);
        finish_item(trav, tree);
    }

///// OBJECT STRATEGIES

    NOINLINE static
    void use_attrs (
        const Traversal& trav, const Tree& tree, const AttrsDcrPrivate* attrs
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
        auto next_list_buf = std::unique_ptr<uint32[]>(new uint32[tree.length + 1]);
        for (uint32 i = 0; i < tree.length; i++) next_list_buf[i] = i;
        next_list_buf[tree.length] = -1;

        claim_attrs_use_attrs(trav, tree, &next_list_buf[0] + 1, attrs);
        if (next_list_buf[0] != uint32(-1)) {
            expect(tree.rep == Rep::Object);
            raise_AttrRejected(
                trav.desc, tree.data.as_object_ptr[next_list_buf[0]].first
            );
        }
    }

    NOINLINE static
    void use_computed_attrs (
        const Traversal& trav, const Tree& tree,
        const Accessor* keys_acr, AttrFunc<Mu>* f
    ) {
         // Computed attrs always take the entire object, so we don't need to
         // allocate a next_list.
        expect(tree.rep == Rep::Object);
        set_keys(trav, TreeObjectSlice(tree), keys_acr);
        expect(tree.rep == Rep::Object);
        for (auto& pair : TreeObjectSlice(tree)) {
            write_computed_attr(trav, pair, f);
        }
        finish_item(trav, tree);
    }

    NOINLINE static
    void claim_attrs (
        const Traversal& trav, const Tree& tree, uint32* next_list
    ) {
        if (auto attrs = trav.desc->attrs()) [[likely]] {
            claim_attrs_use_attrs(trav, tree, next_list, attrs);
        }
        else if (auto keys = trav.desc->keys_acr()) {
            auto f = trav.desc->attr_func()->f;
            claim_attrs_use_computed_attrs(trav, tree, next_list, keys, f);
        }
        else raise_AttrsNotSupported(trav.desc);
    }

    NOINLINE static
    void claim_attrs_use_attrs (
        const Traversal& trav, const Tree& tree, uint32* next_list,
        const AttrsDcrPrivate* attrs
    ) {
        expect(tree.rep == Rep::Object);
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
             // First try matching attr directly even if it's included
            uint32* prev_next; uint32 j;
            for (
                prev_next = &next_list[-1], j = *prev_next;
                j < tree.length;
                prev_next = &next_list[j], j = *prev_next
            ) {
                auto& [key, value] = tree.data.as_object_ptr[j];
                if (key == attr->key) {
                    trav.follow_attr(
                        attr->acr(), attr->key, AccessMode::Write,
                        [&value](const Traversal& child)
                    { traverse(child, value); });
                     // Claim attr by deleting link
                    *prev_next = next_list[j];
                    goto next_attr;
                }
            }
             // No match, try including
            if (attr->acr()->attr_flags & AttrFlags::Include) {
                trav.follow_attr(
                    attr->acr(), attr->key, AccessMode::Write,
                    [&tree, next_list](const Traversal& child)
                {
                    claim_attrs(child, tree, next_list);
                     // The claim_* stack doesn't call finish_item so call it here.
                });
            }
             // Maybe it's optional then?
            else if (attr->acr()->attr_flags & AttrFlags::Optional) {
            }
             // Nope, there's nothing more we can do.
            else raise_AttrMissing(trav.desc, attr->key);
            next_attr:;
        }
        finish_item(trav, tree);
    }

    NOINLINE static
    void claim_attrs_use_computed_attrs (
        const Traversal& trav, const Tree& tree, uint32* next_list,
        const Accessor* keys_acr, AttrFunc<Mu>* f
    ) {
         // We should only get here if a parent item included a child item that
         // has computed attrs.
        expect(tree.rep == Rep::Object);
        set_keys(trav, TreeObjectSlice(tree), keys_acr);
        expect(tree.rep == Rep::Object);
        uint32* prev_next; uint32 i;
        for (
            prev_next = &next_list[-1], i = *prev_next;
            i < tree.length;
            prev_next = &next_list[i], i = *prev_next
        ) {
            write_computed_attr(trav, tree.data.as_object_ptr[i], f);
        }
         // Consume entire list
        next_list[-1] = -1;
        finish_item(trav, tree);
    }

    static
    void set_keys (
        const Traversal& trav, TreeObjectSlice object,
        const Accessor* keys_acr
    ) {
        if (!(keys_acr->flags & AcrFlags::Readonly)) {
             // Writable keys, so write them.
            auto keys = UniqueArray<AnyString>(Capacity(object.size()));
            for (usize i = 0; i < object.size(); i++) {
                keys.emplace_back_expect_capacity(object[i].first);
            }
            keys_acr->write(*trav.address, [&keys](Mu& v){
                reinterpret_cast<AnyArray<AnyString>&>(v) = move(keys);
            });
            expect(!keys.owned());
        }
        else {
             // Readonly keys?  Read them and check that they match.
            AnyArray<AnyString> keys;
            keys_acr->read(*trav.address, [&keys](Mu& v){
                keys = reinterpret_cast<AnyArray<AnyString>&>(v);
            });
#ifndef NDEBUG
             // Check returned keys for duplicates
            for (usize i = 0; i < keys.size(); i++)
            for (usize j = 0; j < i; j++) {
                expect(keys[i] != keys[j]);
            }
#endif
            if (keys.size() >= object.size()) {
                for (auto& required : keys) {
                    for (auto& given : object) {
                        if (given.first == required) goto next_required;
                    }
                    raise_AttrMissing(trav.desc, required);
                    next_required:;
                }
            }
            else [[unlikely]] {
                 // Too many keys given
                for (auto& given : object) {
                    for (auto& required : keys) {
                        if (required == given.first) goto next_given;
                    }
                    raise_AttrRejected(trav.desc, given.first);
                    next_given:;
                }
                never();
            }
        }
    }

    static
    void write_computed_attr (
        const Traversal& trav, const TreePair& pair, AttrFunc<Mu>* f
    ) {
        auto& [key, value] = pair;
        Reference ref = f(*trav.address, key);
        if (!ref) raise_AttrNotFound(trav.desc, key);
        trav.follow_attr_func(
            ref, f, key, AccessMode::Write,
            [&value](const Traversal& child)
        { traverse(child, value); });
    }

///// ARRAY STRATEGIES

    NOINLINE static
    void use_elems (
        const Traversal& trav, const Tree& tree, const ElemsDcrPrivate* elems
    ) {
        expect(tree.rep == Rep::Array);
        auto array = TreeArraySlice(tree);
         // Check whether length is acceptable
        usize min = elems->n_elems;
        while (elems->elem(min-1)->acr()->attr_flags & AttrFlags::Optional) {
            min -= 1;
        }
        if (array.size() < min || array.size() > elems->n_elems) {
            raise_LengthRejected(trav.desc, min, elems->n_elems, array.size());
        }
        for (usize i = 0; i < array.size(); i++) {
            const Tree& child_tree = array[i];
            trav.follow_elem(
                elems->elem(i)->acr(), i, AccessMode::Write,
                [&child_tree](const Traversal& child)
            { traverse(child, child_tree); });
        }
        finish_item(trav, tree);
    }

    NOINLINE static
    void use_computed_elems (
        const Traversal& trav, const Tree& tree,
        const Accessor* length_acr, ElemFunc<Mu>* f
    ) {
        expect(tree.rep == Rep::Array);
        if (!(length_acr->flags & AcrFlags::Readonly)) {
            length_acr->write(*trav.address, [len{tree.length}](Mu& v){
                reinterpret_cast<usize&>(v) = len;
            });
        }
        else {
             // For readonly length, read it and check that it's the same.
            usize len;
            length_acr->read(*trav.address, [&len](Mu& v){
                len = reinterpret_cast<usize&>(v);
            });
            if (tree.length != len) {
                raise_LengthRejected(trav.desc, len, len, tree.length);
            }
        }
        auto array = TreeArraySlice(tree);
        for (usize i = 0; i < array.size(); i++) {
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.desc, i);
            const Tree& child_tree = array[i];
            trav.follow_elem_func(
                ref, f, i, AccessMode::Write,
                [&child_tree](const Traversal& child)
            { traverse(child, child_tree); });
        }
        finish_item(trav, tree);
    }

///// OTHER STRATEGIES

    NOINLINE static
    void use_values (
        const Traversal& trav, const Tree& tree, const ValuesDcrPrivate* values
    ) {
        for (uint i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (tree == value->name) {
                values->assign(*trav.address, *value->get_value());
                return finish_item(trav, tree);
            }
        }
        no_match(trav, tree);
    }

    NOINLINE static
    void use_delegate (
        const Traversal& trav, const Tree& tree, const Accessor* acr
    ) {
        trav.follow_delegate(
            acr, AccessMode::Write, [&tree](const Traversal& child)
        { traverse(child, tree); });
        finish_item(trav, tree);
    }

///// REGISTERING SWIZZLE AND INIT

    NOINLINE static
    void finish_item (const Traversal& trav, const Tree& tree) {
         // Now register swizzle and init ops.  We're doing it now instead of at the
         // beginning to make sure that children get swizzled and initted before
         // their parent.
        if (!!trav.desc->swizzle_offset | !!trav.desc->init_offset) {
            register_swizzle_init(trav, tree);
        }
         // Done
    }

    NOINLINE static
    void register_swizzle_init (const Traversal& trav, const Tree& tree) {
         // We're duplicating the work to get the ref and loc if there's both a
         // swizzle and an init, but almost no types are going to have both.
        if (auto swizzle = trav.desc->swizzle()) {
            IFTContext::current->swizzle_ops.emplace_back(
                swizzle->f, trav.to_reference(), tree, trav.to_location()
            );
        }
        if (auto init = trav.desc->init()) {
            IFTContext::current->init_ops.emplace_back(
                init->f, trav.to_reference(), trav.to_location()
            );
        }
    }

///// ERRORS

    [[noreturn, gnu::cold]] NOINLINE static
    void fail (const Traversal& trav, const Tree& tree) {
         // If we got here, we failed to find any method to from_tree this item.
         // Go through maybe a little too much effort to figure out what went
         // wrong.
        if (tree.form == Form::Error) {
             // Dunno how a lazy error managed to smuggle itself this far.  Give
             // it the attention it deserves.
            std::rethrow_exception(std::exception_ptr(tree));
        }
        bool object_rejected = tree.form == Form::Object &&
            (trav.desc->values() || trav.desc->accepts_array());
        bool array_rejected = tree.form == Form::Array &&
            (trav.desc->values() || trav.desc->accepts_object());
        bool other_rejected =
            trav.desc->accepts_array() || trav.desc->accepts_object();
        if (object_rejected || array_rejected || other_rejected) {
            raise_FromTreeFormRejected(trav.desc, tree.form);
        }
        else if (trav.desc->values()) {
            raise(e_FromTreeValueNotFound, cat(
                "No value for type ", Type(trav.desc).name(),
                " matches the provided tree ", tree_to_string(tree)
            ));
        }
        else raise(e_FromTreeNotSupported, cat(
            "Item of type ", Type(trav.desc).name(), " does not support from_tree."
        ));
    }
};

} using namespace in;

void item_from_tree (
    const Reference& item, const Tree& tree, LocationRef loc,
    ItemFromTreeFlags flags
) {
    TraverseFromTree::start(item, tree, loc, flags);
}

void raise_FromTreeFormRejected (Type t, Form f) {
    raise(e_FromTreeFormRejected, cat(
        "Item of type ", t.name(),
        " does not support from_tree with a tree of form ", item_to_string(&f)
    ));
}

} // ayu
