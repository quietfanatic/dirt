#include "../serialize-from-tree.h"

#include "serialize-compound-private.h"

namespace ayu {
namespace in {

struct SwizzleOp {
    using FP = void(*)(Mu&, const Tree&);
    FP f;
    Reference item;
     // This can't be TreeRef because the referenced Tree could go away after a
     // nested from_tree is called with DELAY_SWIZZLE
    Tree tree;
    Location loc;
};
struct InitOp {
    using FP = void(*)(Mu&);
    FP f;
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

void ser_from_tree (const Traversal& trav, TreeRef tree) {
     // If description has a from_tree, just use that.
    if (auto from_tree = trav.desc->from_tree()) [[likely]] {
        from_tree->f(*trav.address, tree);
        goto done;
    }
     // Now the behavior depends on what kind of tree we've been given
    if (tree->form == Form::Object) {
        if (trav.desc->accepts_object()) {
            expect(tree->rep == Rep::Object);
            auto object = TreeObjectSlice(*tree);
            auto keys = UniqueArray<AnyString>(Capacity(object.size()));
            for (auto& [key, value] : object) {
                keys.emplace_back_expect_capacity(key);
            }
            ser_set_keys(trav, move(keys));
            for (auto& [key, value] : object) {
                ser_attr(trav, key, ACR_WRITE,
                    [value{TreeRef(value)}](const Traversal& child)
                {
                    ser_from_tree(child, value);
                });
            }
            goto done;
        }
    }
    else if (tree->form == Form::Array) {
        if (trav.desc->accepts_array()) {
            expect(tree->rep == Rep::Array);
            auto array = TreeArraySlice(*tree);
            ser_set_length(trav, array.size());
            for (usize i = 0; i < array.size(); i++) {
                ser_elem(trav, i, ACR_WRITE,
                    [elem{TreeRef(array[i])}](const Traversal& child)
                {
                    ser_from_tree(child, elem);
                });
            }
            goto done;
        }
    }
    else if (tree->form == Form::Error) {
         // Dunno how we got this far but whatever
        std::rethrow_exception(std::exception_ptr(*tree));
    }
    else if (auto values = trav.desc->values()) {
         // All other tree types support the values descriptor
        for (uint i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (tree == value->name) {
                values->assign(*trav.address, *value->get_value());
                goto done;
            }
        }
    }
     // Nothing matched, so use delegate
    if (auto acr = trav.desc->delegate_acr()) {
        trav.follow_delegate(acr, ACR_WRITE, [tree](const Traversal& child){
            ser_from_tree(child, tree);
        });
        goto done;
    }
     // Still nothing?  Allow swizzle with no from_tree.
    if (trav.desc->swizzle()) goto done;

     // If we got here, we failed to find any method to from_tree this item.
     // Go through maybe a little too much effort to figure out what went wrong.
    {
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

    done:
     // Now register swizzle and init ops.  We're doing it now instead of at the
     // beginning to make sure that children get swizzled and initted before
     // their parent.
    auto swizzle = trav.desc->swizzle();
    auto init = trav.desc->init();
    if (swizzle || init) [[unlikely]] {
         // We're duplicating the work to get the ref and loc if there's both a
         // swizzle and an init, but almost no types are going to have both.
        if (swizzle) {
            IFTContext::current->swizzle_ops.emplace_back(
                swizzle->f, trav.to_reference(), tree, trav.to_location()
            );
        }
        if (init) {
            IFTContext::current->init_ops.emplace_back(
                init->f, trav.to_reference(), trav.to_location()
            );
        }
    }
}

void IFTContext::do_swizzles () {
     // Swizzling might add more swizzle ops; this will happen if we're
     // swizzling a pointer which points to a separate resource; that resource
     // will be load()ed in op.f().
    while (!swizzle_ops.empty()) {
         // Explicitly assign to clear swizzle_ops
        auto swizzles = move(swizzle_ops);
        for (auto& op : swizzles) {
            PushBaseLocation pbl (op.loc);
             // TODO: wrap error messages
            if (auto address = op.item.address()) {
                op.f(*address, op.tree);
            }
            else op.item.access(ACR_MODIFY, [&op](Mu& v){
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
            PushBaseLocation pbl (op.loc);
            if (auto address = op.item.address()) {
                op.f(*address);
            }
            else op.item.access(ACR_MODIFY, [&op](Mu& v){
                op.f(v);
            });
             // Initting might even add more swizzle ops.
            do_swizzles();
        }
    }
}

IFTContext* IFTContext::current = null;

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
        Traversal::start(item, loc, false, ACR_WRITE,
            [tree](const Traversal& trav)
        { ser_from_tree(trav, tree); });
    }
    else {
        IFTContext context;
        Traversal::start(item, loc, false, ACR_WRITE,
            [tree](const Traversal& trav)
        { ser_from_tree(trav, tree); });
        context.do_swizzles();
        context.do_inits();
    }
}

[[gnu::cold]]
void raise_FromTreeFormRejected (Type t, Form f) {
    raise(e_FromTreeFormRejected, cat(
        "Item of type ", t.name(),
        " does not support from_tree with a tree of form ",
        item_to_string(&f)
    ));
}

} // ayu::in
