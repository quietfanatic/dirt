// This tracks the decisions that were made during a serialization operation.
// It has two purposes:
//   1. Allow creating an AnyRef to the current item in case the current item
//      is not addressable, without having to start over from the very
//      beginning or duplicate work.  This is mainly to support swizzle and
//      init ops.
//   2. Track the current location without any heap allocations, but allow
//      getting an actual heap-allocated Location to the current item if needed
//      for error reporting.

// This is implemented as a class hierarchy, because that ends up being the most
// efficient way to do what we want to do.  Without this, we implement the
// traversal system with a lot of lambdas, but each lambda stores its data on
// the stack separately, leading to a lot of redundant stack usage and passing
// extra pointers around.  By coalescing all of the data for one traversal into
// a single object, we ensure it's all accessible through a single pointer.

#pragma once

#include "../common.h"
#include "../reflection/accessors.private.h"
#include "../reflection/descriptors.private.h"
#include "location.h"
#include "to-tree.h"

namespace ayu::in {

enum class Traversal2Op : uint8 {
    Start,
    Attr,
    ComputedAttr,
    Elem,
    ComputedElem,
    ContiguousElem,
    Delegate,
};

struct Traversal2 {
    const Traversal2* parent;
    const DescriptionPrivate* desc;
     // This address is not guaranteed to be permanently valid unless
     // addressable is set.
    Mu* address;
    Traversal2Op op;
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

     // We could save a little stack space with Start and Delegate by putting
     // these in a class hierarchy instead of unions, but the extra complexity
     // isn't worth it if we're going to subclass this for the different
     // traversal purposes (which itself will save a bunch of stack space).  In
     // theory, one class system can maintain two independent hierarchies by
     // storing one of their data behind the pointer instead of after it, but
     // that's an optimization for another day.
    union {
        const AnyRef* reference; // Start
        const Accessor* acr; // Attr, Elem, Delegate
        AttrFunc<Mu>* attr_func; // ComputedAttr
        ElemFunc<Mu>* elem_func; // ComputedElem
        DataFunc<Mu>* data_func; // ContiguousElem
    };
    union {
         // Store pointer because LocationRef isn't trivially constructible
        const Location* location; // Start
        const StaticString* static_key; // Attr
        const AnyString* key; // ComputedAttr
        usize index; // Elem, ComputedElem, ContiguousElem
    };

    AnyRef to_reference () const noexcept;
    AnyRef to_reference_parent_addressable () const noexcept;
    AnyRef to_reference_chain () const noexcept;
    SharedLocation to_location () const noexcept;
    SharedLocation to_location_chain () const noexcept;
    [[noreturn, gnu::cold]]
    void wrap_exception () const;
};

template <class Self>
struct FollowingTraversal : Traversal2 {
     // This static_cast actually be done until this class is complete, but by
     // then it's too late :<
    //static_assert(requires (Traversal2 t) { static_cast<Self&>(t); });

    template <void(Self::* visit )()>
    void follow_start (
        const AnyRef& ref, LocationRef loc, bool only_addr, AccessMode mode
    ) try {
         // static_cast the invocant, not the method, because static_casting the
         // method won't work if the pointer has to be tweaked (I think).
        auto& self = static_cast<Self&>(*this);
        expect(ref);
        parent = null;
        op = Traversal2Op::Start;
        readonly = ref.host.type.readonly();
        only_addressable = only_addr;
        collapse_optional = false;
        reference = &ref;
        location = loc.data;
         // A lot of AnyRef's methods branch on acr, and while those checks would
         // normally be able to be merged, the indirect calls to the acr's virtual
         // functions invalidate a lot of optimizations, so instead of working
         // directly on the reference, we're going to pick it apart into host and
         // acr.
        if (!ref.acr) [[likely]] {
            desc = DescriptionPrivate::get(ref.host.type);
            address = ref.host.address;
            addressable = true;
            children_addressable = true;
            (self.*visit)();
        }
        else {
            readonly |= !!(ref.acr->flags & AcrFlags::Readonly);
            desc = DescriptionPrivate::get(ref.acr->type(ref.host.address));
            address = ref.acr->address(*ref.host.address);
            if (address) {
                addressable = true;
                children_addressable = true;
                (self.*visit)();
            }
            else {
                addressable = false;
                children_addressable =
                    !!(ref.acr->flags & AcrFlags::PassThroughAddressable);
                if (!only_addressable || children_addressable) {
                     // TODO: merge identical lambdas
                    ref.access(mode, CallbackRef<void(Mu&)>(
                        self, [](Self& self, Mu& v)
                    {
                        self.address = &v;
                        (self.*visit)();
                    }));
                }
            }
        }
    } catch (...) { wrap_exception(); }

    template <void (Self::* visit)()>
    void follow_acr (
        const Traversal2& p, const Accessor* acr_, AccessMode mode
    ) try {
        auto& self = static_cast<Self&>(*this);
        parent = &p;
        readonly = parent->readonly | !!(acr_->flags & AcrFlags::Readonly);
        only_addressable = parent->only_addressable;
        collapse_optional = !!(acr_->attr_flags & AttrFlags::CollapseOptional);
        acr = acr_;
        desc = DescriptionPrivate::get(acr->type(parent->address));
        address = acr->address(*parent->address);
        if (address) [[likely]] {
            addressable = parent->children_addressable;
            children_addressable = parent->children_addressable;
            (self.*visit)();
        }
        else {
            addressable = false;
            children_addressable = parent->children_addressable &
                !!(acr->flags & AcrFlags::PassThroughAddressable);
            if (!only_addressable || children_addressable) {
                acr->access(mode, *parent->address, CallbackRef<void(Mu&)>(
                    self, [](decltype(self)& self, Mu& v)
                {
                    self.address = &v;
                    (self.*visit)();
                }));
            }
        }
    }
    catch (...) {
         // TODO: Not sure whether we should call this on parents or children.
         // We should probably call it on children, but TODO to verify that all
         // members that wrap_exception accesses will be initialized already.
        parent->wrap_exception();
    }

    template <void (Self::* visit)()>
    void follow_ref (
        const Traversal2& p, const AnyRef& ref, AccessMode mode
    ) try {
        auto& self = static_cast<Self&>(*this);
        parent = &p;
        readonly = parent->readonly | ref.host.type.readonly();
        only_addressable = parent->only_addressable;
        collapse_optional = false;
        if (!ref.acr) [[likely]] {
            desc = DescriptionPrivate::get(ref.host.type);
            address = ref.host.address;
            addressable = parent->children_addressable;
            children_addressable = parent->children_addressable;
            (self.*visit)();
        }
        else {
            readonly |= !!(ref.acr->flags & AcrFlags::Readonly);
            desc = DescriptionPrivate::get(ref.acr->type(ref.host.address));
            address = ref.acr->address(*ref.host.address);
            if (address) {
                addressable = parent->children_addressable;
                children_addressable = parent->children_addressable;
                (self.*visit)();
            }
            else {
                addressable = false;
                children_addressable = parent->children_addressable &
                    !!(ref.acr->flags & AcrFlags::PassThroughAddressable);
                if (!only_addressable || children_addressable) {
                    ref.access(mode, CallbackRef<void(Mu&)>(
                        self, [](Self& self, Mu& v)
                    {
                        self.address = &v;
                        (self.*visit)();
                    }));
                }
            }
        }
    }
    catch (...) { parent->wrap_exception(); }

    template <void (Self::* visit)()>
    void follow_ptr (
        const Traversal2& p, AnyPtr ptr, AccessMode
    ) try {
        auto& self = static_cast<Self&>(*this);
        parent = &p;
        readonly = parent->readonly | ptr.type.readonly();
        only_addressable = parent->only_addressable;
        collapse_optional = false;
        desc = DescriptionPrivate::get(ptr.type);
        address = ptr.address;
        addressable = parent->children_addressable;
        children_addressable = parent->children_addressable;
        (self.*visit)();
    }
    catch (...) { parent->wrap_exception(); }

    template <void (Self::* visit)()>
    void follow_attr (
        const Traversal2& p, const Accessor* acr, const StaticString& key,
        AccessMode mode
    ) {
        static_key = &key;
        op = Traversal2Op::Attr;
        follow_acr<visit>(p, acr, mode);
    }

     // key is a passed as a reference instead of a pointer so that a temporary
     // can be passed in.  The pointer will be released when this function
     // returns, so no worry about a dangling pointer to a temporary.
    template <void (Self::* visit)()>
    void follow_computed_attr (
        const Traversal2& p, const AnyRef& ref, AttrFunc<Mu>* func,
        const AnyString& k, AccessMode mode
    ) {
        attr_func = func;
        key = &k;
        op = Traversal2Op::ComputedAttr;
        follow_ref<visit>(p, ref, mode);
    }

    template <void (Self::* visit)()>
    void follow_elem (
        const Traversal2& p, const Accessor* acr, usize index, AccessMode mode
    ) {
        index = index;
        op = Traversal2Op::Elem;
        follow_acr<visit>(p, acr, mode);
    }

    template <void (Self::* visit)()>
    void follow_computed_elem (
        const Traversal2& p, const AnyRef& ref, ElemFunc<Mu>* func,
        usize index, AccessMode mode
    ) {
        func = func;
        index = index;
        op = Traversal2Op::ComputedElem;
        follow_ref<visit>(p, ref, mode);
    }

    template <void (Self::* visit)()>
    void follow_contiguous_elem (
        const Traversal2& p, AnyPtr ptr, DataFunc<Mu>* func,
        usize index, AccessMode mode
    ) {
        func = func;
        index = index;
        op = Traversal2Op::ContiguousElem;
        follow_ptr<visit>(p, ptr, mode);
    }

    template <void (Self::* visit)()>
    void follow_delegate (
        const Traversal2& p, const Accessor* acr, AccessMode mode
    ) {
        op = Traversal2Op::Delegate;
        follow_acr<visit>(p, acr, mode);
    }
};

 // noexcept because any user code called from here should be confirmed to
 // already work without throwing.
inline
AnyRef Traversal2::to_reference () const noexcept {
    if (addressable) {
        return AnyPtr(Type(desc, readonly), address);
    }
    else if (op == Traversal2Op::Start) {
        return *reference;
    }
    else if (parent->addressable) {
        return to_reference_parent_addressable();
    }
    else return to_reference_chain();
}

NOINLINE inline
AnyRef Traversal2::to_reference_parent_addressable () const noexcept {
    switch (op) {
        case Traversal2Op::Delegate: case Traversal2Op::Attr: case Traversal2Op::Elem: {
            auto type = Type(parent->desc, parent->readonly);
            return AnyRef(AnyPtr(type, parent->address), acr);
        }
        case Traversal2Op::ComputedAttr: {
            return attr_func(*parent->address, *key);
        }
        case Traversal2Op::ComputedElem: {
            return elem_func(*parent->address, index);
        }
        case Traversal2Op::ContiguousElem: {
            auto data = data_func(*parent->address);
            auto desc = DescriptionPrivate::get(data.type);
            data.address = (Mu*)(
                (char*)data.address + index * desc->cpp_size
            );
            return data;
        }
        default: never();
    }
}

NOINLINE inline
AnyRef Traversal2::to_reference_chain () const noexcept {
    AnyRef parent_ref = parent->to_reference();
    switch (op) {
        case Traversal2Op::Attr: case Traversal2Op::Elem:
        case Traversal2Op::Delegate: {
            return AnyRef(parent_ref.host, new ChainAcr(
                parent_ref.acr, acr
            ));
        }
        case Traversal2Op::ComputedAttr: {
            return AnyRef(parent_ref.host, new ChainAttrFuncAcr(
                parent_ref.acr, attr_func, *key
            ));
        }
        case Traversal2Op::ComputedElem: {
            return AnyRef(parent_ref.host, new ChainElemFuncAcr(
                parent_ref.acr, elem_func, index
            ));
        }
        case Traversal2Op::ContiguousElem: {
            return AnyRef(parent_ref.host, new ChainDataFuncAcr(
                parent_ref.acr, data_func, index
            ));
        }
        default: never();
    }
}

inline
SharedLocation Traversal2::to_location () const noexcept {
    if (op == Traversal2Op::Start) {
        if (location) return SharedLocation(location);
         // This * took a half a day of debugging to add. :(
        else return SharedLocation(*reference);
    }
    else return to_location_chain();
}

NOINLINE inline
SharedLocation Traversal2::to_location_chain () const noexcept {
    SharedLocation parent_loc = parent->to_location();
    switch (op) {
        case Traversal2Op::Delegate: return parent_loc;
        case Traversal2Op::Attr: {
            return SharedLocation(move(parent_loc), *static_key);
        }
        case Traversal2Op::ComputedAttr: {
            return SharedLocation(move(parent_loc), *key);
        }
        case Traversal2Op::Elem:
        case Traversal2Op::ComputedElem:
        case Traversal2Op::ContiguousElem: {
            return SharedLocation(move(parent_loc), index);
        }
        default: never();
    }
}

inline
void Traversal2::wrap_exception () const {
     // TODO: don't call to_location() if not necessary
    rethrow_with_travloc(to_location());
}

} // namespace ayu::in
