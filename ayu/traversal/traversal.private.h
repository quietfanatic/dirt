#pragma once
#include "../common.h"
#include "../reflection/access.private.h"
#include "../reflection/description.private.h"
#include "route.h"
#include "to-tree.h"

namespace ayu::in {

 // This tracks the decisions that were made during a serialization operation.
 // It's primary purpose is to allow creating an AnyRef to the current item in
 // case the current item is not addressable, but without having to start over
 // from the beginning, and without requiring any heap allocations otherwise.
 //
 // A Traversal has two dimensions of subtyping.  One is the particular step
 // being performed (attr, elem, delegate, etc), here called the TraversalStep.
 // The other one is the overall serialization operation being performed
 // (to_tree, from_tree, scan, etc).  The latter subtypes attach data to the
 // beginning of the Traversal, not the end.
enum class TraversalStep : u8 {
    Start,
    Acr,
    ComputedAttr,
    ComputedElem,
    ContiguousElem,
};
struct Traversal {
    const Traversal* parent;
    Type type;
    Mu* address;
    TraversalStep step;
     // Cumulative access capabilities for all items traversed so far.  This is
     // unused by to_tree traversal, because it only ever does read accesses.
    AccessCaps caps;
     // Extra flags only used by certain traversal stacks, since we have extra
     // room here.
    union {
         // ScanTraversal: Attr containing this item has collapse_optional set.
        bool collapse_optional;
         // ToTreeTraversal: Catch and embed errors instead of throwing them.
        bool embed_errors;
    };

    const DescriptionPrivate* desc () const {
        return DescriptionPrivate::get(type);
    }

    void to_reference (void* r) const noexcept;
    [[noreturn, gnu::cold]]
    void wrap_exception () const;
};

[[noreturn, gnu::cold]]
void rethrow_with_scanned_route (const AnyRef& item);

///// TRAVERSAL SUFFIXES

struct StartTraversal : Traversal {
    const AnyRef* reference;
    RouteRef route;
};

struct AcrTraversal : Traversal {
    const Accessor* acr;
};

struct DelegateTraversal : AcrTraversal { };

struct AttrTraversal : AcrTraversal { };

struct ElemTraversal : AcrTraversal { };

struct RefTraversal : Traversal { };

struct PtrTraversal : Traversal { };

struct ComputedAttrTraversal : RefTraversal {
    AttrFunc<Mu>* func;
    const AnyString* key;
};

struct ComputedElemTraversal : RefTraversal {
    ElemFunc<Mu>* func;
    u32 index;
};

struct ContiguousElemTraversal : PtrTraversal {
    DataFunc<Mu>* func;
    u32 index;
};

///// COMMON TRAVERSAL PREFIX

struct ReturnRefTraversalHead {
    AnyRef* r;
};

template <class T = Traversal>
struct ReturnRefTraversal : ReturnRefTraversalHead, T { };

inline void return_ref (const Traversal& tr) {
    auto& trav = static_cast<const ReturnRefTraversal<>&>(tr);
    expect(!trav.r->acr);
    trav.to_reference(trav.r);
}

///// GENERIC TRAVERSAL FUNCTIONS

using VisitFunc = void(const Traversal&);

template <VisitFunc& visit> NOINLINE
void trav_after_access (Traversal& child, Type t, Mu* v) {
    child.type = t;
    child.address = v;
    visit(child);
}

 // These should always be inlined, because they have a lot of parameters, and
 // their callers are prepared to allocate a lot of stack for them.
template <VisitFunc& visit> ALWAYS_INLINE
void trav_start (
    StartTraversal& child, const AnyRef& ref, RouteRef rt, AccessCaps mode
) try {
    expect(ref);

    child.parent = null;
    child.step = TraversalStep::Start;
    child.reference = &ref;
    child.route = rt;
    child.caps = ref.caps();
    ref.access(mode, AccessCB(
        static_cast<Traversal&>(child),
        &trav_after_access<visit>
    ));
} catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_acr (
    AcrTraversal& child, const Traversal& parent,
    const Accessor* acr, AccessCaps mode
) try {
    child.parent = &parent;
    child.step = TraversalStep::Acr;
    child.caps = parent.caps * acr->caps;
    child.acr = acr;
    acr->access(mode, *parent.address, AccessCB(
        static_cast<Traversal&>(child),
        &trav_after_access<visit>
    ));
}
catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_ref (
    RefTraversal& child, const Traversal& parent,
    const AnyRef& ref, AccessCaps mode
) try {
    child.parent = &parent;
    child.caps = parent.caps * ref.caps();
     // TODO: disassemble this ref to save stack space
    ref.access(mode, AccessCB(
        static_cast<Traversal&>(child),
        &trav_after_access<visit>
    ));
}
catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_ptr (
    PtrTraversal& child, const Traversal& parent,
    AnyPtr ptr, AccessCaps
) try {
    child.parent = &parent;
    child.caps = parent.caps * ptr.caps();
    trav_after_access<visit>(
        child, ptr.type(), ptr.address
    );
}
catch (...) { child.wrap_exception(); }

template <VisitFunc& visit> ALWAYS_INLINE
void trav_attr (
    AcrTraversal& child, const Traversal& parent,
    const Accessor* acr, const StaticString&, AccessCaps mode
) {
    trav_acr<visit>(child, parent, acr, mode);
}

 // key is a reference instead of a pointer so that a temporary can be
 // passed in.  The pointer will be released when this function returns, so
 // no worry about a dangling pointer to a temporary.
template <VisitFunc& visit> ALWAYS_INLINE
void trav_computed_attr (
    ComputedAttrTraversal& child, const Traversal& parent,
    const AnyRef& ref, AttrFunc<Mu>* func, const AnyString& key, AccessCaps mode
) {
    child.step = TraversalStep::ComputedAttr;
    child.func = func;
    child.key = &key;
    trav_ref<visit>(child, parent, ref, mode);
}

template <VisitFunc& visit> ALWAYS_INLINE
void trav_elem (
    ElemTraversal& child, const Traversal& parent,
    const Accessor* acr, u32, AccessCaps mode
) {
    trav_acr<visit>(child, parent, acr, mode);
}

template <VisitFunc& visit> ALWAYS_INLINE
void trav_computed_elem (
    ComputedElemTraversal& child, const Traversal& parent,
    const AnyRef& ref, ElemFunc<Mu>* func, u32 index, AccessCaps mode
) {
    child.step = TraversalStep::ComputedElem;
    child.func = func;
    child.index = index;
    trav_ref<visit>(child, parent, ref, mode);
}

template <VisitFunc& visit> ALWAYS_INLINE
void trav_contiguous_elem (
    ContiguousElemTraversal& child, const Traversal& parent,
    AnyPtr ptr, DataFunc<Mu>* func, u32 index, AccessCaps mode
) {
    child.step = TraversalStep::ContiguousElem;
    child.func = func;
    child.index = index;
    trav_ptr<visit>(child, parent, ptr, mode);
}

template <VisitFunc& visit> ALWAYS_INLINE
void trav_delegate (
    DelegateTraversal& child, const Traversal& parent,
    const Accessor* acr, AccessCaps mode
) {
    trav_acr<visit>(child, parent, acr, mode);
}

} // namespace ayu::in
