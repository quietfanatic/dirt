#include "from-tree.h"

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
                use_attrs(trav, tree, attrs);
            }
            else if (auto keys = trav.desc->keys_acr()) {
                auto f = trav.desc->attr_func()->f;
                use_computed_attrs(trav, tree, keys, f);
            }
            else no_match(trav, tree);
        }
        else if (tree.form == Form::Array) {
            if (trav.desc->accepts_array()) {
                use_array(trav, tree);
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

    NOINLINE static
    void use_from_tree (
        const Traversal& trav, const Tree& tree, FromTreeFunc<Mu>* f
    ) {
        f(*trav.address, tree);
        finish_item(trav, tree);
    }

    NOINLINE static
    void use_attrs (
        const Traversal& trav, const Tree& tree, const AttrsDcrPrivate* attrs
    ) {
        expect(tree.rep == Rep::Object);
        { // Restrict scope of object so its destructor doesn't block tail call
             // Copy so we can destructively remove pairs as we claim them
            auto object = UniqueArray<TreePair>(TreeObjectSlice(tree));
            claim_attrs_use_attrs(trav, object, attrs);
            if (object) raise_AttrRejected(trav.desc, object[0].first);
        }
        finish_item(trav, tree);
    }

    NOINLINE static
    void use_computed_attrs (
        const Traversal& trav, const Tree& tree,
        const Accessor* keys_acr, AttrFunc<Mu>* f
    ) {
        expect(tree.rep == Rep::Object);
        {
            auto object = UniqueArray<TreePair>(TreeObjectSlice(tree));
            claim_attrs_use_computed_attrs(trav, object, keys_acr, f);
            expect(!object);
        }
        finish_item(trav, tree);
    }

    NOINLINE static
    void claim_attrs (
        const Traversal& trav, UniqueArray<TreePair>& object
    ) {
        if (auto attrs = trav.desc->attrs()) {
            claim_attrs_use_attrs(trav, object, attrs);
        }
        else if (auto keys = trav.desc->keys_acr()) {
            auto f = trav.desc->attr_func()->f;
            claim_attrs_use_computed_attrs(trav, object, keys, f);
        }
        else raise_AttrsNotSupported(trav.desc);
    }

    NOINLINE static
    void claim_attrs_use_attrs (
        const Traversal& trav, UniqueArray<TreePair>& object,
        const AttrsDcrPrivate* attrs
    ) {
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
             // First try matching attr directly even if it's included
            for (usize i = 0; i < object.size(); i++) {
                if (object[i].first == attr->key) {
                    Tree& value = object[i].second;
                    trav.follow_attr(
                        attr->acr(), attr->key, AccessMode::Write,
                        [&value](const Traversal& child)
                    { traverse(child, value); });
                    object.erase(i);
                    goto next_attr;
                }
            }
             // No match, try including
            if (attr->acr()->attr_flags & AttrFlags::Include) {
                trav.follow_attr(
                    attr->acr(), attr->key, AccessMode::Write,
                    [&object](const Traversal& child)
                { claim_attrs(child, object); });
            }
             // Maybe it's optional then?
            else if (attr->acr()->attr_flags & AttrFlags::Optional) {
            }
             // Nope, there's nothing more we can do.
            else raise_AttrMissing(trav.desc, attr->key);
            next_attr:;
        }
    }

    NOINLINE static
    void claim_attrs_use_computed_attrs (
        const Traversal& trav, UniqueArray<TreePair>& object,
        const Accessor* keys_acr, AttrFunc<Mu>* f
    ) {
         // Set keys
        Type keys_type = keys_acr->type(trav.address);
        if (keys_type == Type::CppType<AnyArray<AnyString>>()) {
            set_keys_AnyArray(trav, object, keys_acr);
        }
        else [[unlikely]] {
            set_keys_generic(trav, object, keys_acr, keys_type);
        }
         // Write each attr
        for (usize i = 0; i < object.size(); i++) {
            Reference ref = f(*trav.address, object[i].first);
            if (!ref) raise_AttrNotFound(trav.desc, object[i].first);
            Tree& value = object[i].second;
            trav.follow_attr_func(
                ref, f, object[i].first, AccessMode::Write,
                [&value](const Traversal& child)
            { traverse(child, value); });
             // consume
            TreePair(move(object[i]));
        }
         // object should be fully consumed.
        object.unsafe_set_size(0);
    }

    static
    void set_keys_AnyArray (
        const Traversal& trav, UniqueArray<TreePair>& object,
        const Accessor* keys_acr
    ) {
        auto keys = UniqueArray<AnyString>(Capacity(object.size()));
        for (usize i = 0; i < object.size(); i++) {
            keys.emplace_back_expect_capacity(object[i].first);
        }
        keys_acr->write(*trav.address, [&keys](Mu& v){
            reinterpret_cast<AnyArray<AnyString>&>(v) = move(keys);
        });
        expect(!keys.owned());
    }

    static
    void set_keys_generic (
        const Traversal& trav, UniqueArray<TreePair>& object,
        const Accessor* keys_acr, Type keys_type
    ) {
        auto array = UniqueArray<Tree>(Capacity(object.size()));
        for (usize i = 0; i < object.size(); i++) {
            array.emplace_back_expect_capacity(Tree(object[i].first));
        }
        auto keys = Tree(move(array));
        keys_acr->write(*trav.address, [keys_type, &keys](Mu& v){
            item_from_tree(Reference(keys_type, &v), keys);
        });
    }

    NOINLINE static
    void use_array (const Traversal& trav, const Tree& tree) {
        expect(tree.rep == Rep::Array);
        auto array = TreeArraySlice(tree);
        trav_set_length(trav, array.size());
        for (usize i = 0; i < array.size(); i++) {
            const Tree& elem = array[i];
            trav_elem(trav, i, AccessMode::Write,
                [&elem](const Traversal& child)
            { traverse(child, elem); });
        }
        finish_item(trav, tree);
    }

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
