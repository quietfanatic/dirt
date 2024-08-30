#pragma once

#include "../common.h"
#include "../reflection/accessors.private.h"
#include "../reflection/descriptors.private.h"
#include "location.h"
#include "to-tree.h"

namespace ayu::in {

 // This tracks the decisions that were made during a serialization operation.
 // It has two purposes:
 //   1. Allow creating an AnyRef to the current item in case the current item
 //      is not addressable, without having to start over from the very
 //      beginning or duplicate work.  This is mainly to support swizzle and
 //      init ops.
 //   2. Track the current location without any heap allocations, but allow
 //      getting an actual heap-allocated Location to the current item if needed
 //      for error reporting.
enum class TraversalOp : uint8 {
    Start,
    Delegate,
    Attr,
    ComputedAttr,
    Elem,
    ComputedElem,
    ContiguousElem,
};
struct Traversal {
    const Traversal* parent;
    const DescriptionPrivate* desc;
    TraversalOp op;
     // This address is not guaranteed to be permanently valid unless
     // addressable is set.
    Mu* address;
     // Type can keep track of readonly, but DescriptionPrivate* can't, so keep
     // track of it here.
    bool readonly;
     // Only traverse addressable items.  If an unaddressable and
     // non-pass-through item is encountered, the traversal's callback will not
     // be called.
    bool only_addressable;
     // Attr has collapse_optional flag set
    bool collapse_optional;
     // If this item has a stable address, then to_reference() can use the
     // address directly instead of having to chain from parent.
    bool addressable;
     // Set if parent->children_addressable and pass_through_addressable.  This
     // can go from on to off, but never from off to on.
    bool children_addressable;

    AnyRef to_reference () const noexcept;
    AnyRef to_reference_parent_addressable () const noexcept;
    AnyRef to_reference_chain () const noexcept;
    SharedLocation to_location () const noexcept;
    SharedLocation to_location_chain () const noexcept;
    [[noreturn, gnu::cold]]
    void wrap_exception () const;
};

struct StartTraversal : Traversal {
    const AnyRef* reference;
    LocationRef location;
};

struct AcrTraversal : Traversal {
    const Accessor* acr;
};

struct RefTraversal : Traversal { };

struct PtrTraversal : Traversal { };

struct DelegateTraversal : AcrTraversal { };

struct AttrTraversal : AcrTraversal {
    const StaticString* key;
};

struct ElemTraversal : AcrTraversal {
    usize index;
};

struct ComputedAttrTraversal : RefTraversal {
    AttrFunc<Mu>* func;
    const AnyString* key;
};

struct ComputedElemTraversal : RefTraversal {
    ElemFunc<Mu>* func;
    usize index;
};

struct ContiguousElemTraversal : PtrTraversal {
    DataFunc<Mu>* func;
    usize index;
};

using VisitFunc = void(const Traversal&);

 // All the lambdas in these functions were identical, so deduplicate them into
 // this function.  It's only two instructions long but it's still better for
 // branch prediction for it not to be duplicated.
template <VisitFunc& visit>
void set_address_and_visit (Traversal& child, Mu& v) {
    child.address = &v;
    visit(child);
}

 // These should always be inlined, because they have a lot of parameters, and
 // their callers are prepared to allocate a lot of stack for them.
template <VisitFunc& visit> ALWAYS_INLINE
void trav_start (
    StartTraversal& child,
    const AnyRef& ref, LocationRef loc, bool only_addressable, AccessMode mode
) try {
    expect(ref);

    child.parent = null;
    child.readonly = ref.host.type.readonly();
    child.only_addressable = only_addressable;
    child.collapse_optional = false;
    child.op = TraversalOp::Start;
    child.reference = &ref;
    child.location = loc;
     // A lot of AnyRef's methods branch on acr, and while those checks would
     // normally be able to be merged, the indirect calls to the acr's virtual
     // functions invalidate a lot of optimizations, so instead of working
     // directly on the reference, we're going to pick it apart into host and
     // acr.
    if (!ref.acr) [[likely]] {
        child.desc = DescriptionPrivate::get(ref.host.type);
        child.address = ref.host.address;
        child.addressable = true;
        child.children_addressable = true;
        visit(child);
    }
    else {
        child.readonly |= !!(ref.acr->flags & AcrFlags::Readonly);
        child.desc = DescriptionPrivate::get(ref.acr->type(ref.host.address));
        if (!(ref.acr->flags & AcrFlags::Unaddressable)) {
            child.address = ref.acr->address(*ref.host.address);
            child.addressable = true;
            child.children_addressable = true;
            visit(child);
        }
        else {
            child.addressable = false;
            child.children_addressable =
                !!(ref.acr->flags & AcrFlags::PassThroughAddressable);
            if (!child.only_addressable || child.children_addressable) {
                ref.access(mode, CallbackRef<void(Mu&)>(
                    static_cast<Traversal&>(child), &set_address_and_visit<visit>
                ));
            }
        }
    }
} catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_acr (
    AcrTraversal& child, const Traversal& parent,
    const Accessor* acr, AccessMode mode
) try {
    child.parent = &parent;
    child.readonly = parent.readonly | !!(acr->flags & AcrFlags::Readonly);
    child.only_addressable = parent.only_addressable;
    child.collapse_optional = !!(acr->attr_flags & AttrFlags::CollapseOptional);
    child.acr = acr;
    child.desc = DescriptionPrivate::get(acr->type(parent.address));
    if (!(acr->flags & AcrFlags::Unaddressable)) [[likely]] {
        child.address = acr->address(*parent.address);
        child.addressable = parent.children_addressable;
        child.children_addressable = parent.children_addressable;
        visit(child);
    }
    else {
        child.addressable = false;
        child.children_addressable = parent.children_addressable &
            !!(acr->flags & AcrFlags::PassThroughAddressable);
        if (!child.only_addressable || child.children_addressable) {
            acr->access(mode, *parent.address, CallbackRef<void(Mu&)>(
                static_cast<Traversal&>(child), &set_address_and_visit<visit>
            ));
        }
    }
}
catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_ref (
    RefTraversal& child, const Traversal& parent,
    const AnyRef& ref, AccessMode mode
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
        visit(child);
    }
    else {
        child.readonly |= !!(ref.acr->flags & AcrFlags::Readonly);
        child.desc = DescriptionPrivate::get(ref.acr->type(ref.host.address));
        if (!(ref.acr->flags & AcrFlags::Unaddressable)) {
            child.address = ref.acr->address(*ref.host.address);
            child.addressable = parent.children_addressable;
            child.children_addressable = parent.children_addressable;
            visit(child);
        }
        else {
            child.addressable = false;
            child.children_addressable = parent.children_addressable &
                !!(ref.acr->flags & AcrFlags::PassThroughAddressable);
            if (!child.only_addressable || child.children_addressable) {
                ref.access(mode, CallbackRef<void(Mu&)>(
                    static_cast<Traversal&>(child), &set_address_and_visit<visit>
                ));
            }
        }
    }
}
catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_ptr (
    PtrTraversal& child, const Traversal& parent,
    AnyPtr ptr, AccessMode
) try {
    child.parent = &parent;
    child.readonly = parent.readonly | ptr.type.readonly();
    child.only_addressable = parent.only_addressable;
    child.collapse_optional = false;
    child.desc = DescriptionPrivate::get(ptr.type);
    child.address = ptr.address;
    child.addressable = parent.children_addressable;
    child.children_addressable = parent.children_addressable;
    visit(child);
}
catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_attr (
    AttrTraversal& child, const Traversal& parent,
    const Accessor* acr, const StaticString& key, AccessMode mode
) {
    child.op = TraversalOp::Attr;
    child.key = &key;
    trav_acr<visit>(child, parent, acr, mode);
}

 // key is a reference instead of a pointer so that a temporary can be
 // passed in.  The pointer will be released when this function returns, so
 // no worry about a dangling pointer to a temporary.
template <VisitFunc& visit> ALWAYS_INLINE
void trav_computed_attr (
    ComputedAttrTraversal& child, const Traversal& parent,
    const AnyRef& ref, AttrFunc<Mu>* func, const AnyString& key, AccessMode mode
) {
    child.op = TraversalOp::ComputedAttr;
    child.func = func;
    child.key = &key;
    trav_ref<visit>(child, parent, ref, mode);
}

template <VisitFunc& visit> ALWAYS_INLINE
void trav_elem (
    ElemTraversal& child, const Traversal& parent,
    const Accessor* acr, usize index, AccessMode mode
) {
    child.op = TraversalOp::Elem;
    child.index = index;
    trav_acr<visit>(child, parent, acr, mode);
}

template <VisitFunc& visit> ALWAYS_INLINE
void trav_computed_elem (
    ComputedElemTraversal& child, const Traversal& parent,
    const AnyRef& ref, ElemFunc<Mu>* func, usize index, AccessMode mode
) {
    child.op = TraversalOp::ComputedElem;
    child.func = func;
    child.index = index;
    trav_ref<visit>(child, parent, ref, mode);
}

template <VisitFunc& visit> ALWAYS_INLINE
void trav_contiguous_elem (
    ContiguousElemTraversal& child, const Traversal& parent,
    AnyPtr ptr, DataFunc<Mu>* func, usize index, AccessMode mode
) {
    child.op = TraversalOp::ContiguousElem;
    child.func = func;
    child.index = index;
    trav_ptr<visit>(child, parent, ptr, mode);
}

template <VisitFunc& visit> ALWAYS_INLINE
void trav_delegate (
    DelegateTraversal& child, const Traversal& parent,
    const Accessor* acr, AccessMode mode
) {
    child.op = TraversalOp::Delegate;
    trav_acr<visit>(child, parent, acr, mode);
}

 // noexcept because any user code called from here should be confirmed to
 // already work without throwing.
inline
AnyRef Traversal::to_reference () const noexcept {
    if (addressable) {
        return AnyPtr(Type(desc, readonly), address);
    }
    else if (op == TraversalOp::Start) {
        auto& self = static_cast<const StartTraversal&>(*this);
        return *self.reference;
    }
    else if (parent->addressable) {
        return to_reference_parent_addressable();
    }
    else return to_reference_chain();
}

NOINLINE inline
AnyRef Traversal::to_reference_parent_addressable () const noexcept {
    switch (op) {
        case TraversalOp::Delegate: case TraversalOp::Attr: case TraversalOp::Elem: {
            auto& self = static_cast<const AcrTraversal&>(*this);
            auto type = Type(parent->desc, parent->readonly);
            return AnyRef(AnyPtr(type, parent->address), self.acr);
        }
        case TraversalOp::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(*this);
            return self.func(*parent->address, *self.key);
        }
        case TraversalOp::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(*this);
            return self.func(*parent->address, self.index);
        }
        case TraversalOp::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(*this);
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
AnyRef Traversal::to_reference_chain () const noexcept {
    AnyRef parent_ref = parent->to_reference();
    switch (op) {
        case TraversalOp::Attr: case TraversalOp::Elem:
        case TraversalOp::Delegate: {
            auto& self = static_cast<const AcrTraversal&>(*this);
            return AnyRef(parent_ref.host, new ChainAcr(
                parent_ref.acr, self.acr
            ));
        }
        case TraversalOp::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(*this);
            return AnyRef(parent_ref.host, new ChainAttrFuncAcr(
                parent_ref.acr, self.func, *self.key
            ));
        }
        case TraversalOp::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(*this);
            return AnyRef(parent_ref.host, new ChainElemFuncAcr(
                parent_ref.acr, self.func, self.index
            ));
        }
        case TraversalOp::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(*this);
            return AnyRef(parent_ref.host, new ChainDataFuncAcr(
                parent_ref.acr, self.func, self.index
            ));
        }
        default: never();
    }
}

NOINLINE inline
SharedLocation Traversal::to_location () const noexcept {
    if (op == TraversalOp::Start) {
        auto& self = static_cast<const StartTraversal&>(*this);
        if (self.location) return self.location;
         // This * took a half a day of debugging to add. :(
        else return SharedLocation(*self.reference);
    }
    else return to_location_chain();
}

NOINLINE inline
SharedLocation Traversal::to_location_chain () const noexcept {
    SharedLocation parent_loc = parent->to_location();
    switch (op) {
        case TraversalOp::Delegate: return parent_loc;
        case TraversalOp::Attr: {
            auto& self = static_cast<const AttrTraversal&>(*this);
            return SharedLocation(move(parent_loc), *self.key);
        }
        case TraversalOp::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(*this);
            return SharedLocation(move(parent_loc), *self.key);
        }
         // These three branches can technically be merged, hopefully the
         // compiler does so.
        case TraversalOp::Elem: {
            auto& self = static_cast<const ElemTraversal&>(*this);
            return SharedLocation(move(parent_loc), self.index);
        }
        case TraversalOp::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(*this);
            return SharedLocation(move(parent_loc), self.index);
        }
        case TraversalOp::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(*this);
            return SharedLocation(move(parent_loc), self.index);
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
