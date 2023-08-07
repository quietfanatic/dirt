#include "../serialize-from-tree.h"

#include "serialize-compound-private.h"

namespace ayu {
namespace in {

struct SwizzleOp {
    SwizzleFunc<Mu>* f;
    Reference item;
     // This can't be TreeRef because the referenced Tree could go away after a
     // nested from_tree is called with DELAY_SWIZZLE
    Tree tree;
    Location loc;
};
struct InitOp {
    InitFunc<Mu>* f;
    Reference item;
    Location loc;
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
    void do_swizzles ();
    void do_inits ();
};

void ser_from_tree (const Traversal&, TreeRef);
void ser_from_tree_from_tree (const Traversal&, TreeRef, FromTreeFunc<Mu>*);
void ser_from_tree_object (const Traversal&, TreeRef);
void ser_from_tree_array (const Traversal&, TreeRef);
void ser_from_tree_values (const Traversal&, TreeRef, const ValuesDcrPrivate*);
void ser_from_tree_after_values (const Traversal&, TreeRef);
void ser_from_tree_delegate (const Traversal&, TreeRef, const Accessor*);
void ser_from_tree_finish (const Traversal&, TreeRef);
void ser_from_tree_swizzle_init (const Traversal&, TreeRef);
[[noreturn]] void ser_from_tree_fail (const Traversal&, TreeRef);

} using namespace in;

void item_from_tree (
    const Reference& item, TreeRef tree, LocationRef loc,
    ItemFromTreeFlags flags
) {
    PushBaseLocation pbl(*loc ? *loc : Location(item));
    if (tree->form == Form::Undefined) {
        raise(e_FromTreeFormRejected, "Undefined tree given to item_from_tree");
    }
    if (flags & DELAY_SWIZZLE && IFTContext::current) {
         // Delay swizzle and inits to the outer item_from_tree call.  Basically
         // this just means keep the current context instead of making a new
         // one.
        Traversal::start(item, loc, false, AccessMode::Write,
            [tree](const Traversal& trav)
        { ser_from_tree(trav, tree); });
    }
    else {
        IFTContext context;
        Traversal::start(item, loc, false, AccessMode::Write,
            [tree](const Traversal& trav)
        { ser_from_tree(trav, tree); });
        context.do_swizzles();
        context.do_inits();
    }
}

namespace in {

NOINLINE
void ser_from_tree (const Traversal& trav, TreeRef tree) {
     // If description has a from_tree, just use that.
    if (auto from_tree = trav.desc->from_tree()) [[likely]] {
        ser_from_tree_from_tree(trav, tree, from_tree->f);
    }
     // Now the behavior depends on what kind of tree we've been given
    else if (tree->form == Form::Object) {
        if (trav.desc->accepts_object()) {
            ser_from_tree_object(trav, tree);
        }
        else ser_from_tree_after_values(trav, tree);
    }
    else if (tree->form == Form::Array) {
        if (trav.desc->accepts_array()) {
            ser_from_tree_array(trav, tree);
        }
        else ser_from_tree_after_values(trav, tree);
    }
    else if (auto values = trav.desc->values()) {
         // All other tree types support the values descriptor
        ser_from_tree_values(trav, tree, values);
    }
    else ser_from_tree_after_values(trav, tree);
}

NOINLINE
void ser_from_tree_from_tree (
    const Traversal& trav, TreeRef tree, FromTreeFunc<Mu>* f
) {
    f(*trav.address, tree);
    ser_from_tree_finish(trav, tree);
}

NOINLINE
void ser_from_tree_object (const Traversal& trav, TreeRef tree) {
    expect(tree->rep == Rep::Object);
    auto object = TreeObjectSlice(*tree);
    {
        auto keys = UniqueArray<AnyString>(Capacity(object.size()));
        for (auto& [key, value] : object) {
            keys.emplace_back_expect_capacity(key);
        }
        ser_set_keys(trav, move(keys));
         // Restrict scope of keys to here
    }
    for (auto& [key, value] : object) {
        ser_attr(trav, key, AccessMode::Write,
            [value{TreeRef(value)}](const Traversal& child)
        { ser_from_tree(child, value); });
    }
    ser_from_tree_finish(trav, tree);
}

NOINLINE
void ser_from_tree_array (const Traversal& trav, TreeRef tree) {
    expect(tree->rep == Rep::Array);
    auto array = TreeArraySlice(*tree);
    ser_set_length(trav, array.size());
    for (usize i = 0; i < array.size(); i++) {
        ser_elem(trav, i, AccessMode::Write,
            [elem{TreeRef(array[i])}](const Traversal& child)
        { ser_from_tree(child, elem); });
    }
    ser_from_tree_finish(trav, tree);
}

NOINLINE
void ser_from_tree_values (
    const Traversal& trav, TreeRef tree, const ValuesDcrPrivate* values
) {
    for (uint i = 0; i < values->n_values; i++) {
        auto value = values->value(i);
        if (tree == value->name) {
            values->assign(*trav.address, *value->get_value());
            return ser_from_tree_finish(trav, tree);
        }
    }
    ser_from_tree_after_values(trav, tree);
}

NOINLINE
void ser_from_tree_after_values (
    const Traversal& trav, TreeRef tree
) {
     // Nothing matched, so try delegate
    if (auto acr = trav.desc->delegate_acr()) {
        ser_from_tree_delegate(trav, tree, acr);
    }
     // Still nothing?  Allow swizzle with no from_tree.
    else if (trav.desc->swizzle()) {
        ser_from_tree_swizzle_init(trav, tree);
    }
    else ser_from_tree_fail(trav, tree);
}

NOINLINE
void ser_from_tree_delegate (
    const Traversal& trav, TreeRef tree, const Accessor* acr
) {
    trav.follow_delegate(
        acr, AccessMode::Write, [tree](const Traversal& child)
    { ser_from_tree(child, tree); });
    ser_from_tree_finish(trav, tree);
}

NOINLINE
void ser_from_tree_finish (const Traversal& trav, TreeRef tree) {
     // Now register swizzle and init ops.  We're doing it now instead of at the
     // beginning to make sure that children get swizzled and initted before
     // their parent.
    if (!!trav.desc->swizzle_offset | !!trav.desc->init_offset) {
        ser_from_tree_swizzle_init(trav, tree);
    }
     // Done
}

NOINLINE
void ser_from_tree_swizzle_init (const Traversal& trav, TreeRef tree) {
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

[[gnu::cold]] NOINLINE
void ser_from_tree_fail (const Traversal& trav, TreeRef tree) {
     // If we got here, we failed to find any method to from_tree this item.
     // Go through maybe a little too much effort to figure out what went wrong.
    if (tree->form == Form::Error) {
         // Dunno how a lazy error managed to smuggle itself this far.  Give it
         // the show it deserves.
        std::rethrow_exception(std::exception_ptr(*tree));
    }
    bool object_rejected = tree->form == Form::Object &&
        (trav.desc->values() || trav.desc->accepts_array());
    bool array_rejected = tree->form == Form::Array &&
        (trav.desc->values() || trav.desc->accepts_object());
    bool other_rejected =
        trav.desc->accepts_array() || trav.desc->accepts_object();
    if (object_rejected || array_rejected || other_rejected) {
        raise_FromTreeFormRejected(trav.desc, tree->form);
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

void IFTContext::do_swizzles () {
     // Swizzling might add more swizzle ops; this will happen if we're
     // swizzling a pointer which points to a separate resource; that resource
     // will be load()ed in op.f().
    while (!swizzle_ops.empty()) {
         // Explicitly assign to clear swizzle_ops
        auto swizzles = move(swizzle_ops);
        for (auto& op : swizzles) {
            expect(op.loc);
            PushBaseLocation pbl (op.loc);
             // TODO: wrap error messages
            op.item.access(AccessMode::Modify, [&op](Mu& v){
                op.f(v, op.tree);
            });
        }
    }
}
void IFTContext::do_inits () {
     // Initting might add some more init ops.  It'd be weird, but it's allowed
     // for an init() to load another resource.
    while (!init_ops.empty()) {
        auto inits = move(init_ops);
        for (auto& op : inits) {
            expect(op.loc);
            PushBaseLocation pbl (op.loc);
            op.item.access(AccessMode::Modify, [&op](Mu& v){
                op.f(v);
            });
             // Initting might even add more swizzle ops.
            do_swizzles();
        }
    }
}

IFTContext* IFTContext::current = null;

} using namespace in;

[[gnu::cold]]
void raise_FromTreeFormRejected (Type t, Form f) {
    raise(e_FromTreeFormRejected, cat(
        "Item of type ", t.name(),
        " does not support from_tree with a tree of form ",
        item_to_string(&f)
    ));
}

} // ayu
