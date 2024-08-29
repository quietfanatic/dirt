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
enum class Traversal3Op : uint8 {
    Start,
    Delegate,
    Attr,
    ComputedAttr,
    Elem,
    ComputedElem,
    ContiguousElem,
};
struct Traversal3 {
    const Traversal3* parent;
    const DescriptionPrivate* desc;
    Traversal3Op op;
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

struct StartTraversal3 : Traversal3 {
    const AnyRef* reference;
    LocationRef location;
};

struct AcrTraversal3 : Traversal3 {
    const Accessor* acr;
};

struct RefTraversal3 : Traversal3 { };

struct PtrTraversal3 : Traversal3 { };

struct DelegateTraversal3 : AcrTraversal3 { };

struct AttrTraversal3 : AcrTraversal3 {
    const StaticString* key;
};

struct ElemTraversal3 : AcrTraversal3 {
    usize index;
};

struct ComputedAttrTraversal3 : RefTraversal3 {
    AttrFunc<Mu>* func;
    const AnyString* key;
};

struct ComputedElemTraversal3 : RefTraversal3 {
    ElemFunc<Mu>* func;
    usize index;
};

struct ContiguousElemTraversal3 : PtrTraversal3 {
    DataFunc<Mu>* func;
    usize index;
};

using VisitFunc = void(const Traversal3&);

 // All the lambdas in these functions were identical, so deduplicate them into
 // this function.  It's only two instructions long but it's still better for
 // branch prediction for it not to be duplicated.
template <VisitFunc& visit>
void set_address_and_visit (Traversal3& child, Mu& v) {
    child.address = &v;
    visit(child);
}

template <VisitFunc& visit>
void trav_start (
    StartTraversal3& child,
    const AnyRef& ref, LocationRef loc, bool only_addressable, AccessMode mode
) try {
    expect(ref);

    child.parent = null;
    child.readonly = ref.host.type.readonly();
    child.only_addressable = only_addressable;
    child.collapse_optional = false;
    child.op = Traversal3Op::Start;
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
        child.address = ref.acr->address(*ref.host.address);
        if (child.address) {
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
                    static_cast<Traversal3&>(child), &set_address_and_visit<visit>
                ));
            }
        }
    }
} catch (...) { child.wrap_exception(); }

template <VisitFunc& visit>
void trav_acr (
    AcrTraversal3& child, const Traversal3& parent,
    const Accessor* acr, AccessMode mode
) try {
    child.parent = &parent;
    child.readonly = parent.readonly | !!(acr->flags & AcrFlags::Readonly);
    child.only_addressable = parent.only_addressable;
    child.collapse_optional = !!(acr->attr_flags & AttrFlags::CollapseOptional);
    child.acr = acr;
    child.desc = DescriptionPrivate::get(acr->type(parent.address));
    child.address = acr->address(*parent.address);
    if (child.address) [[likely]] {
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
                static_cast<Traversal3&>(child), &set_address_and_visit<visit>
            ));
        }
    }
}
 // TODO: catch on child?
catch (...) { parent.wrap_exception(); }

template <VisitFunc& visit>
void trav_ref (
    RefTraversal3& child, const Traversal3& parent,
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
        child.address = ref.acr->address(*ref.host.address);
        if (child.address) {
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
                    static_cast<Traversal3&>(child), &set_address_and_visit<visit>
                ));
            }
        }
    }
}
catch (...) { parent.wrap_exception(); }

template <VisitFunc& visit>
void trav_ptr (
    PtrTraversal3& child, const Traversal3& parent,
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
catch (...) { parent.wrap_exception(); }

template <VisitFunc& visit>
void trav_attr (
    AttrTraversal3& child, const Traversal3& parent,
    const Accessor* acr, const StaticString& key, AccessMode mode
) {
    child.op = Traversal3Op::Attr;
    child.key = &key;
    trav_acr<visit>(child, parent, acr, mode);
}

 // key is a reference instead of a pointer so that a temporary can be
 // passed in.  The pointer will be released when this function returns, so
 // no worry about a dangling pointer to a temporary.
template <VisitFunc& visit>
void trav_computed_attr (
    ComputedAttrTraversal3& child, const Traversal3& parent,
    const AnyRef& ref, AttrFunc<Mu>* func, const AnyString& key, AccessMode mode
) {
    child.op = Traversal3Op::ComputedAttr;
    child.func = func;
    child.key = &key;
    trav_ref<visit>(child, parent, ref, mode);
}

template <VisitFunc& visit>
void trav_elem (
    ElemTraversal3& child, const Traversal3& parent,
    const Accessor* acr, usize index, AccessMode mode
) {
    child.op = Traversal3Op::Elem;
    child.index = index;
    trav_acr<visit>(child, parent, acr, mode);
}

template <VisitFunc& visit>
void trav_computed_elem (
    ComputedElemTraversal3& child, const Traversal3& parent,
    const AnyRef& ref, ElemFunc<Mu>* func, usize index, AccessMode mode
) {
    child.op = Traversal3Op::ComputedElem;
    child.func = func;
    child.index = index;
    trav_ref<visit>(child, parent, ref, mode);
}

template <VisitFunc& visit>
void trav_contiguous_elem (
    ContiguousElemTraversal3& child, const Traversal3& parent,
    AnyPtr ptr, DataFunc<Mu>* func, usize index, AccessMode mode
) {
    child.op = Traversal3Op::ContiguousElem;
    child.func = func;
    child.index = index;
    trav_ptr<visit>(child, parent, ptr, mode);
}

template <VisitFunc& visit>
void trav_delegate (
    DelegateTraversal3& child, const Traversal3& parent,
    const Accessor* acr, AccessMode mode
) {
    child.op = Traversal3Op::Delegate;
    trav_acr<visit>(child, parent, acr, mode);
}

 // noexcept because any user code called from here should be confirmed to
 // already work without throwing.
NOINLINE inline
AnyRef Traversal3::to_reference () const noexcept {
    if (addressable) {
        return AnyPtr(Type(desc, readonly), address);
    }
    else if (op == Traversal3Op::Start) {
        auto& self = static_cast<const StartTraversal3&>(*this);
        return *self.reference;
    }
    else if (parent->addressable) {
        return to_reference_parent_addressable();
    }
    else return to_reference_chain();
}

NOINLINE inline
AnyRef Traversal3::to_reference_parent_addressable () const noexcept {
    switch (op) {
        case Traversal3Op::Delegate: case Traversal3Op::Attr: case Traversal3Op::Elem: {
            auto& self = static_cast<const AcrTraversal3&>(*this);
            auto type = Type(parent->desc, parent->readonly);
            return AnyRef(AnyPtr(type, parent->address), self.acr);
        }
        case Traversal3Op::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal3&>(*this);
            return self.func(*parent->address, *self.key);
        }
        case Traversal3Op::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal3&>(*this);
            return self.func(*parent->address, self.index);
        }
        case Traversal3Op::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal3&>(*this);
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
AnyRef Traversal3::to_reference_chain () const noexcept {
    AnyRef parent_ref = parent->to_reference();
    switch (op) {
        case Traversal3Op::Attr: case Traversal3Op::Elem:
        case Traversal3Op::Delegate: {
            auto& self = static_cast<const AcrTraversal3&>(*this);
            return AnyRef(parent_ref.host, new ChainAcr(
                parent_ref.acr, self.acr
            ));
        }
        case Traversal3Op::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal3&>(*this);
            return AnyRef(parent_ref.host, new ChainAttrFuncAcr(
                parent_ref.acr, self.func, *self.key
            ));
        }
        case Traversal3Op::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal3&>(*this);
            return AnyRef(parent_ref.host, new ChainElemFuncAcr(
                parent_ref.acr, self.func, self.index
            ));
        }
        case Traversal3Op::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal3&>(*this);
            return AnyRef(parent_ref.host, new ChainDataFuncAcr(
                parent_ref.acr, self.func, self.index
            ));
        }
        default: never();
    }
}

NOINLINE inline
SharedLocation Traversal3::to_location () const noexcept {
    if (op == Traversal3Op::Start) {
        auto& self = static_cast<const StartTraversal3&>(*this);
        if (self.location) return self.location;
         // This * took a half a day of debugging to add. :(
        else return SharedLocation(*self.reference);
    }
    else return to_location_chain();
}

NOINLINE inline
SharedLocation Traversal3::to_location_chain () const noexcept {
    SharedLocation parent_loc = parent->to_location();
    switch (op) {
        case Traversal3Op::Delegate: return parent_loc;
        case Traversal3Op::Attr: {
            auto& self = static_cast<const AttrTraversal3&>(*this);
            return SharedLocation(move(parent_loc), *self.key);
        }
        case Traversal3Op::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal3&>(*this);
            return SharedLocation(move(parent_loc), *self.key);
        }
         // These three branches can technically be merged, hopefully the
         // compiler does so.
        case Traversal3Op::Elem: {
            auto& self = static_cast<const ElemTraversal3&>(*this);
            return SharedLocation(move(parent_loc), self.index);
        }
        case Traversal3Op::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal3&>(*this);
            return SharedLocation(move(parent_loc), self.index);
        }
        case Traversal3Op::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal3&>(*this);
            return SharedLocation(move(parent_loc), self.index);
        }
        default: never();
    }
}

inline
void Traversal3::wrap_exception () const {
     // TODO: don't call to_location() if not necessary
    rethrow_with_travloc(to_location());
}

} // namespace ayu::in
