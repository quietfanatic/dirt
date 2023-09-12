#pragma once

#include "../common.h"
#include "../reflection/accessors.private.h"
#include "../reflection/descriptors.private.h"
#include "location.h"
#include "to-tree.h"

namespace ayu::in {

 // This tracks the decisions that were made during a serialization operation.
 // It has two purposes:
 //   1. Allow creating a Reference to the current item in case the current item
 //     is not addressable, without having to start over from the very beginning
 //     or duplicate work.  This is mainly to support swizzle and init ops.
 //   2. Track the current location without any heap allocations, but allow
 //     getting an actual heap-allocated Location to the current item if needed
 //     for error reporting.
enum TraversalOp : uint8 {
    START,
    DELEGATE,
    ATTR,
    ATTR_FUNC,
    ELEM,
    ELEM_FUNC,
};
struct Traversal {
    const Traversal* parent;
    Mu* address;
    const DescriptionPrivate* desc;
     // Type can keep track of readonly, but DescriptionPrivate* can't, so keep
     // track of it here.
    bool readonly;
     // If this item has a stable address, then to_reference() can use the
     // address directly instead of having to chain from parent.
    bool addressable;
     // Set if this item has pass_through_addressable AND parent->addressable is
     // true.
    bool children_addressable;
     // Only traverse addressable items.  If an unaddressable and
     // non-pass-through item is encountered, the traversal's callback will not
     // be called.
    bool only_addressable;
    TraversalOp op;
    union {
         // START
        const Reference* reference;
         // DELEGATE, ATTR, ELEM
        const Accessor* acr;
         // ATTR_FUNC
        AttrFunc<Mu>* attr_func;
         // ELEM_FUNC
        ElemFunc<Mu>* elem_func;
    };
    union {
         // START
        LocationRef location;
         // ATTR
        const StaticString* static_key;
         // ATTR_FUNC
        const AnyString* any_key;
         // ELEM, ELEM_FUNC
        usize index;
    };
    void* callback;

    template <class CB>
    static void start (
        const Reference& ref, LocationRef loc, bool only_addressable,
        AccessMode mode, CB cb
    ) {
        expect(ref);
        Traversal child;
        try {

        child.parent = null;
        child.only_addressable = only_addressable;
        child.op = START;
        child.reference = &ref;
        child.location = loc;
         // A lot of Reference's methods branch on acr, so do that branch here
         // to intentionally merge them.
        if (!ref.acr) {
            child.address = ref.host.address;
            child.desc = DescriptionPrivate::get(ref.host.type);
            child.readonly = ref.host.type.readonly();
            child.addressable = true;
            child.children_addressable = true;
            cb(child);
        }
        else if ((child.address = ref.address())) {
            child.desc = DescriptionPrivate::get(ref.type());
            child.readonly = ref.readonly();
            child.addressable = true;
            child.children_addressable = true;
            cb(child);
        }
        else {
            child.desc = DescriptionPrivate::get(ref.type());
            child.readonly = ref.readonly();
            child.addressable = false;
            child.children_addressable =
                ref.acr->flags & AcrFlags::PassThroughAddressable;
            if (!child.only_addressable || child.children_addressable) {
                child.callback = (void*)&cb;
                ref.access(mode, CallbackRef<void(Mu&)>(
                    child, [](Traversal& child, Mu& v)
                {
                    child.address = &v;
                    (*(std::remove_reference_t<CB>*)child.callback)(child);
                }));
            }
        }

        } catch (...) { child.wrap_exception(); }
    }

    template <class CB>
    void follow_acr (
        Traversal& child, const Accessor* acr, AccessMode mode, CB cb
    ) const try {
        child.parent = this;
        child.readonly = readonly | !!(acr->flags & AcrFlags::Readonly);
        child.only_addressable = only_addressable;
        child.acr = acr;
        child.desc = DescriptionPrivate::get(acr->type(address));
        child.address = acr->address(*address);
        if (child.address) {
            child.addressable = children_addressable;
            child.children_addressable = children_addressable;
            cb(child);
        }
        else {
            child.addressable = false;
            child.children_addressable =
                acr->flags & AcrFlags::PassThroughAddressable;
            if (!child.only_addressable || child.children_addressable) {
                child.callback = (void*)&cb;
                acr->access(mode, *address, CallbackRef<void(Mu&)>(
                    child, [](Traversal& child, Mu& v)
                {
                    child.address = &v;
                    (*(std::remove_reference_t<CB>*)child.callback)(child);
                }));
            }
        }
    }
    catch (...) { wrap_exception(); }

    template <class CB>
    void follow_reference (
        Traversal& child, const Reference& ref, AccessMode mode, CB cb
    ) const try {
        child.parent = this;
        child.only_addressable = only_addressable;
        if (!ref.acr) {
            child.address = ref.host.address;
            child.desc = DescriptionPrivate::get(ref.host.type);
            child.readonly = readonly | ref.host.type.readonly();
            child.addressable = children_addressable;
            child.children_addressable = children_addressable;
            cb(child);
        }
        else if ((child.address = ref.address())) {
            child.desc = DescriptionPrivate::get(ref.type());
            child.readonly = readonly || ref.readonly();
            child.addressable = children_addressable;
            child.children_addressable = children_addressable;
            cb(child);
        }
        else {
            child.desc = DescriptionPrivate::get(ref.type());
            child.readonly = readonly || ref.readonly();
            child.addressable = false;
            child.children_addressable =
                ref.acr->flags & AcrFlags::PassThroughAddressable;
            if (!child.only_addressable || child.children_addressable) {
                child.callback = (void*)&cb;
                ref.access(mode, CallbackRef<void(Mu&)>(
                    child, [](Traversal& child, Mu& v)
                {
                    child.address = &v;
                    (*(std::remove_reference_t<CB>*)child.callback)(child);
                }));
            }
        }
    }
    catch (...) { wrap_exception(); }

    template <class CB>
    void follow_delegate (
        const Accessor* acr, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = DELEGATE;
        follow_acr(child, acr, mode, cb);
    }

     // key is a reference instead of a pointer so that a temporary can be
     // passed in.  The pointer will be released when this function returns, so
     // no worry about a dangling pointer to a temporary.
    template <class CB>
    void follow_attr (
        const Accessor* acr, const StaticString& key, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = ATTR;
        child.static_key = &key;
        follow_acr(child, acr, mode, cb);
    }

    template <class CB>
    void follow_attr_func (
        const Reference& ref, AttrFunc<Mu>* func,
        const AnyString& key, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = ATTR_FUNC;
        child.attr_func = func;
        child.any_key = &key;
        follow_reference(child, ref, mode, cb);
    }

    template <class CB>
    void follow_elem (
        const Accessor* acr, usize index, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = ELEM;
        child.index = index;
        follow_acr(child, acr, mode, cb);
    }

    template <class CB>
    void follow_elem_func (
        const Reference& ref, ElemFunc<Mu>* func,
        usize index, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = ELEM_FUNC;
        child.elem_func = func;
        child.index = index;
        follow_reference(child, ref, mode, cb);
    }

     // noexcept because any user code called from here should be confirmed to
     // already work without throwing.
    Reference to_reference () const noexcept {
        if (addressable) {
            return Pointer(Type(desc, readonly), address);
        }
        else if (op == START) {
            return *reference;
        }
        else if (parent->addressable) {
            return to_reference_parent_addressable();
        }
        else return to_reference_chain();
    }
    Reference to_reference_parent_addressable () const noexcept {
        switch (op) {
            case DELEGATE: case ATTR: case ELEM: {
                auto type = Type(parent->desc, parent->readonly);
                return Reference(Pointer(type, parent->address), acr);
            }
            case ATTR_FUNC:
                return attr_func(*parent->address, *any_key);
            case ELEM_FUNC:
                return elem_func(*parent->address, index);
            default: never();
        }
    }
    NOINLINE Reference to_reference_chain () const noexcept {
        Reference parent_ref = parent->to_reference();
        const Accessor* child_acr;
        switch (op) {
            case DELEGATE: case ATTR: case ELEM:
                child_acr = new ChainAcr(parent_ref.acr, acr);
                break;
            case ATTR_FUNC:
                child_acr = new AttrFuncAcr(*attr_func, *any_key);
                break;
            case ELEM_FUNC:
                child_acr = new ElemFuncAcr(*elem_func, index);
                break;
            default: never();
        }
        return Reference(parent_ref.host, child_acr);
    }

    Location to_location () const noexcept {
        if (op == START) {
            if (*location) return location;
             // This * took a half a day of debugging to add. :(
            else return Location(*reference);
        }
        else return to_location_chain();
    }
    NOINLINE Location to_location_chain () const noexcept {
        Location parent_loc = parent->to_location();
        switch (op) {
            case DELEGATE: return parent_loc;
            case ATTR:
                return Location(move(parent_loc), *static_key);
            case ATTR_FUNC:
                return Location(move(parent_loc), *any_key);
            case ELEM: case ELEM_FUNC:
                return Location(move(parent_loc), index);
            default: never();
        }
    }

    [[noreturn, gnu::cold]] void wrap_exception () const {
         // TODO: don't call to_location() if not necessary
        rethrow_with_travloc(to_location());
    }
};

} // namespace ayu::in
