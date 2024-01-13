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
    COMPUTED_ATTR,
    ELEM,
    COMPUTED_ELEM,
    CONTIGUOUS_ELEM,
};
struct Traversal {
    const Traversal* parent;
    const DescriptionPrivate* desc;
    Mu* address;
     // Type can keep track of readonly, but DescriptionPrivate* can't, so keep
     // track of it here.
    bool readonly;
     // Only traverse addressable items.  If an unaddressable and
     // non-pass-through item is encountered, the traversal's callback will not
     // be called.
    bool only_addressable;
     // Parent has collapse_optional flag set
    bool collapse_optional;
     // If this item has a stable address, then to_reference() can use the
     // address directly instead of having to chain from parent.
    bool addressable;
     // Set if parent->children_addressable and pass_through_addressable.  This
     // can go from on to off, but never from off to on.
    bool children_addressable;
    TraversalOp op;
    Reference to_reference () const noexcept;
    Reference to_reference_parent_addressable () const noexcept;
    Reference to_reference_chain () const noexcept;
    Location to_location () const noexcept;
    Location to_location_chain () const noexcept;
    [[noreturn, gnu::cold]]
    void wrap_exception () const;
};

struct StartTraversal : Traversal {
    const Reference* reference;
    LocationRef location;
};

struct AcrTraversal : Traversal {
    const Accessor* acr;
};

struct DelegateTraversal : AcrTraversal { };

struct AttrTraversal : AcrTraversal {
    const StaticString* key;
};

struct ElemTraversal : AcrTraversal {
    usize index;
};

struct AttrFuncTraversal : Traversal {
    AttrFunc<Mu>* func;
    const AnyString* key;
};

struct ElemFuncTraversal : Traversal {
    ElemFunc<Mu>* func;
    usize index;
};

struct DataFuncTraversal : Traversal {
    DataFunc<Mu>* func;
    usize index;
};

template <class Base, class CB>
struct CBTraversal : Base {
    const std::remove_reference_t<CB>* callback;
};

template <class CB>
static void trav_start (
    const Reference& ref, LocationRef loc, bool only_addressable,
    AccessMode mode, const CB& cb
) {
    expect(ref);
    CBTraversal<StartTraversal, CB> child;
    try {

    child.parent = null;
    child.readonly = ref.host.type.readonly();
    child.only_addressable = only_addressable;
    child.collapse_optional = false;
    child.op = START;
    child.reference = &ref;
    child.location = loc;
     // A lot of Reference's methods branch on acr, and while those checks
     // would normally be able to be merged, the indirect calls to the acr's
     // virtual functions invalidate a lot of optimizations, so instead of
     // working directly on the reference, we're going to pick it apart into
     // host and acr.
    if (!ref.acr) [[likely]] {
        child.desc = DescriptionPrivate::get(ref.host.type);
        child.address = ref.host.address;
        child.addressable = true;
        child.children_addressable = true;
        cb(child);
    }
    else {
        child.readonly |= !!(ref.acr->flags & AcrFlags::Readonly);
        child.desc = DescriptionPrivate::get(ref.acr->type(ref.host.address));
        child.address = ref.acr->address(*ref.host.address);
        if (child.address) {
            child.addressable = true;
            child.children_addressable = true;
            cb(child);
        }
        else {
            child.addressable = false;
            child.children_addressable =
                ref.acr->flags & AcrFlags::PassThroughAddressable;
            if (!child.only_addressable || child.children_addressable) {
                 // Optimize callback storage by using the stack object we
                 // already have as a closure, instead of making a new one.
                child.callback = &cb;
                ref.access(mode, CallbackRef<void(Mu&)>(
                    child, [](CBTraversal<StartTraversal, CB>& child, Mu& v)
                {
                    child.address = &v;
                    (*child.callback)(child);
                }));
            }
        }
    }

    } catch (...) { child.wrap_exception(); }
}

template <class Child, class CB>
void trav_acr (
    const Traversal& parent, Child& child,
    const Accessor* acr, AccessMode mode, const CB& cb
) try {
    child.parent = &parent;
    child.readonly = parent.readonly | !!(acr->flags & AcrFlags::Readonly);
    child.only_addressable = parent.only_addressable;
    child.collapse_optional = acr->attr_flags & AttrFlags::CollapseOptional;
    child.acr = acr;
    child.desc = DescriptionPrivate::get(acr->type(parent.address));
    child.address = acr->address(*parent.address);
    if (child.address) {
        child.addressable = parent.children_addressable;
        child.children_addressable = parent.children_addressable;
        cb(child);
    }
    else {
        child.addressable = false;
        child.children_addressable = parent.children_addressable &
            !!(acr->flags & AcrFlags::PassThroughAddressable);
        if (!child.only_addressable || child.children_addressable) {
            child.callback = &cb;
            acr->access(mode, *parent.address, CallbackRef<void(Mu&)>(
                child, [](Child& child, Mu& v)
            {
                child.address = &v;
                (*child.callback)(child);
            }));
        }
    }
}
catch (...) { parent.wrap_exception(); }

template <class Child, class CB>
void trav_reference (
    const Traversal& parent, Child& child,
    const Reference& ref, AccessMode mode, const CB& cb
) try {
    child.parent = &parent;
    child.readonly = parent.readonly | ref.host.type.readonly();
    child.only_addressable = parent.only_addressable;
    child.collapse_optional = false;
    if (!ref.acr) [[likely]] {
        child.desc = DescriptionPrivate::get(ref.host.type);
        child.address = ref.host.address;
        child.addressable = parent.children_addressable;
        child.children_addressable = parent.children_addressable;
        cb(child);
    }
    else {
        child.readonly |= !!(ref.acr->flags & AcrFlags::Readonly);
        child.desc = DescriptionPrivate::get(ref.acr->type(ref.host.address));
        child.address = ref.acr->address(*ref.host.address);
        if (child.address) {
            child.addressable = parent.children_addressable;
            child.children_addressable = parent.children_addressable;
            cb(child);
        }
        else {
            child.addressable = false;
            child.children_addressable = parent.children_addressable &
                !!(ref.acr->flags & AcrFlags::PassThroughAddressable);
            if (!child.only_addressable || child.children_addressable) {
                child.callback = &cb;
                ref.access(mode, CallbackRef<void(Mu&)>(
                    child, [](Child& child, Mu& v)
                {
                    child.address = &v;
                    (*child.callback)(child);
                }));
            }
        }
    }
}
catch (...) { parent.wrap_exception(); }

template <class Child, class CB>
void trav_pointer (
    const Traversal& parent, Child& child,
    Pointer ptr, AccessMode, const CB& cb
) try {
    child.parent = &parent;
    child.readonly = parent.readonly | ptr.type.readonly();
    child.only_addressable = parent.only_addressable;
    child.collapse_optional = false;
    child.desc = DescriptionPrivate::get(ptr.type);
    child.address = ptr.address;
    child.addressable = parent.children_addressable;
    child.children_addressable = parent.children_addressable;
    cb(child);
}
catch (...) { parent.wrap_exception(); }

template <class CB>
void trav_delegate (
    const Traversal& parent, const Accessor* acr, AccessMode mode, const CB& cb
) {
    CBTraversal<DelegateTraversal, CB> child;
    child.op = DELEGATE;
    trav_acr(parent, child, acr, mode, cb);
}

 // key is a reference instead of a pointer so that a temporary can be
 // passed in.  The pointer will be released when this function returns, so
 // no worry about a dangling pointer to a temporary.
template <class CB>
void trav_attr (
    const Traversal& parent, const Accessor* acr, const StaticString& key,
    AccessMode mode, const CB& cb
) {
    CBTraversal<AttrTraversal, CB> child;
    child.op = ATTR;
    child.key = &key;
    trav_acr(parent, child, acr, mode, cb);
}

template <class CB>
void trav_computed_attr (
    const Traversal& parent, const Reference& ref, AttrFunc<Mu>* func,
    const AnyString& key, AccessMode mode, const CB& cb
) {
    CBTraversal<AttrFuncTraversal, CB> child;
    child.op = COMPUTED_ATTR;
    child.func = func;
    child.key = &key;
    trav_reference(parent, child, ref, mode, cb);
}

template <class CB>
void trav_elem (
    const Traversal& parent, const Accessor* acr, usize index,
    AccessMode mode, const CB& cb
) {
    CBTraversal<ElemTraversal, CB> child;
    child.op = ELEM;
    child.index = index;
    trav_acr(parent, child, acr, mode, cb);
}

template <class CB>
void trav_computed_elem (
    const Traversal& parent, const Reference& ref, ElemFunc<Mu>* func,
    usize index, AccessMode mode, const CB& cb
) {
    CBTraversal<ElemFuncTraversal, CB> child;
    child.op = COMPUTED_ELEM;
    child.func = func;
    child.index = index;
    trav_reference(parent, child, ref, mode, cb);
}

template <class CB>
void trav_contiguous_elem (
    const Traversal& parent, Pointer ptr, DataFunc<Mu>* func,
    usize index, AccessMode mode, const CB& cb
) {
     // Don't need to store the CB
    DataFuncTraversal child;
    child.op = CONTIGUOUS_ELEM;
    child.func = func;
    child.index = index;
    trav_pointer(parent, child, ptr, mode, cb);
}

 // noexcept because any user code called from here should be confirmed to
 // already work without throwing.
inline
Reference Traversal::to_reference () const noexcept {
    if (addressable) {
        return Pointer(Type(desc, readonly), address);
    }
    else if (op == START) {
        auto& self = static_cast<const StartTraversal&>(*this);
        return *self.reference;
    }
    else if (parent->addressable) {
        return to_reference_parent_addressable();
    }
    else return to_reference_chain();
}

NOINLINE inline
Reference Traversal::to_reference_parent_addressable () const noexcept {
    switch (op) {
        case DELEGATE: case ATTR: case ELEM: {
            auto& self = static_cast<const AcrTraversal&>(*this);
            auto type = Type(parent->desc, parent->readonly);
            return Reference(Pointer(type, parent->address), self.acr);
        }
        case COMPUTED_ATTR: {
            auto& self = static_cast<const AttrFuncTraversal&>(*this);
            return self.func(*parent->address, *self.key);
        }
        case COMPUTED_ELEM: {
            auto& self = static_cast<const ElemFuncTraversal&>(*this);
            return self.func(*parent->address, self.index);
        }
        case CONTIGUOUS_ELEM: {
            auto& self = static_cast<const DataFuncTraversal&>(*this);
            auto data = self.func(*parent->address);
            auto desc = DescriptionPrivate::get(data.type);
            data.address = (Mu*)(
                (char*)data.address + self.index * desc->cpp_size
            );
            return data;
        }
        default: never();
    }
}

NOINLINE inline
Reference Traversal::to_reference_chain () const noexcept {
    Reference parent_ref = parent->to_reference();
    const Accessor* child_acr;
    switch (op) {
        case DELEGATE: case ATTR: case ELEM: {
            auto& self = static_cast<const AcrTraversal&>(*this);
            child_acr = self.acr;
            child_acr->inc();
            break;
        }
        case COMPUTED_ATTR: {
            auto& self = static_cast<const AttrFuncTraversal&>(*this);
            child_acr = new AttrFuncAcr(self.func, *self.key);
            break;
        }
        case COMPUTED_ELEM: {
            auto& self = static_cast<const ElemFuncTraversal&>(*this);
            child_acr = new ElemFuncAcr(self.func, self.index);
            break;
        }
        case CONTIGUOUS_ELEM: {
            auto& self = static_cast<const DataFuncTraversal&>(*this);
            child_acr = new DataFuncAcr(self.func, self.index);
            break;
        }
        default: never();
    }
     // If parent doesn't have an acr, we should be in
     // to_reference_parent_addressable, not here.
    expect(parent_ref.acr);
    parent_ref.acr->inc();
    return Reference(parent_ref.host, new ChainAcr(parent_ref.acr, child_acr));
}

inline
Location Traversal::to_location () const noexcept {
    if (op == START) {
        auto& self = static_cast<const StartTraversal&>(*this);
        if (*self.location) return self.location;
         // This * took a half a day of debugging to add. :(
        else return Location(*self.reference);
    }
    else return to_location_chain();
}

NOINLINE inline
Location Traversal::to_location_chain () const noexcept {
    Location parent_loc = parent->to_location();
    switch (op) {
        case DELEGATE: return parent_loc;
        case ATTR: {
            auto& self = static_cast<const AttrTraversal&>(*this);
            return Location(move(parent_loc), *self.key);
        }
        case COMPUTED_ATTR: {
            auto& self = static_cast<const AttrFuncTraversal&>(*this);
            return Location(move(parent_loc), *self.key);
        }
        case ELEM: {
            auto& self = static_cast<const ElemTraversal&>(*this);
            return Location(move(parent_loc), self.index);
        }
        case COMPUTED_ELEM: {
            auto& self = static_cast<const ElemFuncTraversal&>(*this);
            return Location(move(parent_loc), self.index);
        }
        case CONTIGUOUS_ELEM: {
            auto& self = static_cast<const DataFuncTraversal&>(*this);
            return Location(move(parent_loc), self.index);
        }
        default: never();
    }
}

inline
void Traversal::wrap_exception () const {
     // TODO: don't call to_location() if not necessary
    rethrow_with_travloc(to_location());
}

} // namespace ayu::in
