#pragma once
#include "description.internal.h"

namespace ayu::in {

template <class C>
const C* offset_get (const void* base, u16 offset) {
    if (!offset) return null;
    return (const C*)((char*)base + offset);
}

struct ValueDcrPrivate : ValueDcr<Mu> {
    Mu* get_value () const {
         // Can't static cast to ValueDcrWithValue<Mu> because it has an inline
         // member of incomplete type Mu.
        auto r = (Mu*)((char*)this + sizeof(ValueDcr<Mu>));
        if (name.flags % TreeFlags::ValueIsPtr) {
            r = *(Mu**)r;
        }
        return r;
    }
};

struct ValuesDcrPrivate : ValuesDcr<Mu> {
    const ValueDcrPrivate* value (u16 i) const {
        u16 offset = (&n_values)[i+1];
        return (const ValueDcrPrivate*)((char*)this + offset);
    }
};

struct AttrDcrPrivate : AttrDcr<Mu> {
    const Accessor* acr () const {
         // We have to take a somewhat roundabout way to get a pointer to acr,
         // because we can't instantiate AttrDcrWith<Mu, Accessor> because
         // Accessor is abstract, and we can't pretend it's a concrete Accessor
         // type because then the optimizer will devirtualize method calls
         // incorrectly (this may no longer be true now that we're using our own
         // vtables, but it's still good to be precise).
         //
         // The Accessor should be right after the attr base in memory, without
         // any padding.  This should be the case if vtable pointers have the
         // same alignment as StaticString and there's nothing else funny going
         // on.
        static_assert(sizeof(AttrDcr<Mu>) % alignof(Accessor) == 0);
        return (const Accessor*)((char*)this + sizeof(AttrDcr<Mu>));
    }
    const Tree* default_value () const {
        if (acr()->attr_flags % AttrFlags::HasDefault) {
            return (const Tree*)((char*)this - sizeof(Tree));
        }
        else return null;
    }
};

struct AttrsDcrPrivate : AttrsDcr<Mu> {
    const AttrDcrPrivate* attr (u16 i) const {
        u16 offset = (&n_attrs)[i+1];
        return (const AttrDcrPrivate*)((char*)this + offset);
    }
};

struct ElemDcrPrivate : ElemDcr<Mu> {
    const Accessor* acr () const {
         // This is much easier than attrs...as long as we don't add
         // anything to ElemDcr...
        return (const Accessor*)this;
    }
};

struct ElemsDcrPrivate : ElemsDcr<Mu> {
    const ElemDcrPrivate* elem (u16 i) const {
        u16 offset = (&n_elems)[i+1];
        return (const ElemDcrPrivate*)((char*)this + offset);
    }
     // Take elements off the end that have the given flag (e.g. optional or
     // invisible).  This could be done at compile-time, but it'd take extra
     // space in the description.
    u16 chop_flag (AttrFlags flag) const {
        u16 r = n_elems;
        while (r && elem(r-1)->acr()->attr_flags % flag) r--;
        return r;
    }
};

struct DescriptionPrivate : DescriptionHeaderFor<Mu> {

    static const DescriptionPrivate* get (Type t) {
        return reinterpret_cast<const DescriptionPrivate*>(t.data);
    }

    const ToTreeDcr<Mu>* to_tree () const {
        return offset_get<ToTreeDcr<Mu>>(this, to_tree_offset);
    }
    const FromTreeDcr<Mu>* from_tree () const {
        return offset_get<FromTreeDcr<Mu>>(this, from_tree_offset);
    }
    const BeforeFromTreeDcr<Mu>* before_from_tree () const {
        return offset_get<BeforeFromTreeDcr<Mu>>(this, before_from_tree_offset);
    }
    const SwizzleDcr<Mu>* swizzle () const {
        return offset_get<SwizzleDcr<Mu>>(this, swizzle_offset);
    }
    const InitDcr<Mu>* init () const {
        return offset_get<InitDcr<Mu>>(this, init_offset);
    }
    const ValuesDcrPrivate* values () const {
        return offset_get<ValuesDcrPrivate>(this, values_offset);
    }
    const AttrsDcrPrivate* attrs () const {
        return offset_get<AttrsDcrPrivate>(this, attrs_offset);
    }
    const ElemsDcrPrivate* elems () const {
        return offset_get<ElemsDcrPrivate>(this, elems_offset);
    }
    const Accessor* keys_acr () const {
        return offset_get<Accessor>(this, keys_offset);
    }
    const ComputedAttrsDcr<Mu>* computed_attrs () const {
        return offset_get<ComputedAttrsDcr<Mu>>(this, computed_attrs_offset);
    }
    const Accessor* length_acr () const {
        return offset_get<Accessor>(this, length_offset);
    }
    const ComputedElemsDcr<Mu>* computed_elems () const {
        return offset_get<ComputedElemsDcr<Mu>>(this, computed_elems_offset);
    }
    const ContiguousElemsDcr<Mu>* contiguous_elems () const {
        return offset_get<ContiguousElemsDcr<Mu>>(this, contiguous_elems_offset);
    }
    const Accessor* delegate_acr () const {
        return offset_get<Accessor>(this, delegate_offset);
    }

    bool accepts_object () const {
        return !!attrs_offset | !!keys_offset;
    }
    bool accepts_array () const {
        return !!elems_offset | !!length_offset;
    }
     // Figure out whether this description prefers being serialized as an array
     // or as an object.  Whichever has a related facet specified first will
     // be picked.
    DescFlags preference () const {
         // We've bumped this calculation up to compile-time.
        return flags & DescFlags::Preference;
    }
};

} // namespace ayu::in
