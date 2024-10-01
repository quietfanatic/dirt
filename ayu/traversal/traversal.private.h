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
    TraversalOp op;
     // Attr has collapse_optional flag set.  This is independent for each item.
    bool collapse_optional;
     // If this item has a stable address, then to_reference() can use the
     // address directly instead of having to chain from parent.
     // Before access, = parent.children_addressable.
     // After access, &= item addressable
    bool addressable;
     // Set if parent->children_addressable and pass_through_addressable.  This
     // can go from on to off, but never from off to on.
     // Before access, = acr pass through addressable (false if no acr)
     // After access, |= child.addressable
    bool children_addressable;
     // Type can keep track of readonly, but DescriptionPrivate* can't, so keep
     // track of it here.  This can go from off to on, but never from on to off.
    bool readonly;
    const DescriptionPrivate* desc;
     // This address is not guaranteed to be permanently valid unless
     // addressable is set.
    Mu* address;

    void to_reference (void* r) const noexcept;
    void to_location (void* r) const noexcept;
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
 // this function.
template <VisitFunc& visit> NOINLINE
void visit_after_access (Traversal& child, AnyPtr v, bool addr) {
    child.addressable &= addr;
    child.children_addressable |= child.addressable;
    child.readonly |= v.type.readonly();
    child.desc = DescriptionPrivate::get(v.type);
    child.address = v.address;
    visit(child);
}

 // These should always be inlined, because they have a lot of parameters, and
 // their callers are prepared to allocate a lot of stack for them.
template <VisitFunc& visit> ALWAYS_INLINE
void trav_start (
    StartTraversal& child, const AnyRef& ref, LocationRef loc, AccessMode mode
) try {
    expect(ref);

    child.parent = null;
    child.op = TraversalOp::Start;
    child.readonly = ref.host.type.readonly();
    child.collapse_optional = false; // Can't collapse top-level item
    child.addressable = true;
    child.children_addressable = ref.acr &&
        !!(ref.acr->flags & AcrFlags::PassThroughAddressable);
    child.reference = &ref;
    child.location = loc;
    ref.access(mode, AccessCB(
        static_cast<Traversal&>(child),
        &visit_after_access<visit>
    ));
} catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_acr (
    AcrTraversal& child, const Traversal& parent,
    const Accessor* acr, AccessMode mode
) try {
    child.acr = acr;
    child.parent = &parent;
    child.collapse_optional = !!(acr->attr_flags & AttrFlags::CollapseOptional);
    child.addressable = parent.children_addressable;
    child.children_addressable =
        !!(acr->flags & AcrFlags::PassThroughAddressable);
    child.readonly = parent.readonly | !!(acr->flags & AcrFlags::Readonly);
    acr->access(mode, *parent.address, AccessCB(
        static_cast<Traversal&>(child),
        &visit_after_access<visit>
    ));
}
catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_ref (
    RefTraversal& child, const Traversal& parent,
    const AnyRef& ref, AccessMode mode
) try {
    child.parent = &parent;
    child.collapse_optional = false;
    child.addressable = parent.children_addressable;
    child.children_addressable = ref.acr &&
        !!(ref.acr->flags & AcrFlags::PassThroughAddressable);
    child.readonly = parent.readonly | ref.host.type.readonly();
    ref.access(mode, AccessCB(
        static_cast<Traversal&>(child),
        &visit_after_access<visit>
    ));
}
catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_ptr (
    PtrTraversal& child, const Traversal& parent,
    AnyPtr ptr, AccessMode
) try {
    child.parent = &parent;
    child.collapse_optional = false;
    child.addressable = parent.children_addressable;
    child.children_addressable = parent.children_addressable;
    child.readonly = parent.readonly | ptr.type.readonly();
    child.desc = DescriptionPrivate::get(ptr.type);
    child.address = ptr.address;
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

inline
void Traversal::wrap_exception () const {
     // TODO: don't call to_location() if not necessary
    SharedLocation loc;
    to_location(&loc);
    rethrow_with_travloc(loc);
}

} // namespace ayu::in
