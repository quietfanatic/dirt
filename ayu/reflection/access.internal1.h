#pragma once
#include "access.h"
#include "../common.h"
#include "../data/tree.h"

namespace ayu::in {

enum class AcrFlags {
     // Writes through this accessor will fail.  Attrs and elems with this
     // accessor will not be serialized.
    Readonly = u8(AC::Write), // Inverted!
     // Consider this item unaddressable even if it normally would be.
    Unaddressable = u8(AC::Address), // Inverted!
     // Children considered addressable even if this item is not addressable.
    ChildrenAddressable = u8(AC::AddressChildren), // Not inverted!

     // These are only used in the describe API.  They're transferred to actual
     // TreeFlags when the ACR is written.
    PreferHex = u8(TreeFlags::PreferHex) << 8,
    PreferCompact = u8(TreeFlags::PreferCompact) << 8,
    PreferExpanded = u8(TreeFlags::PreferExpanded) << 8,
};
DECLARE_ENUM_BITWISE_OPERATORS(AcrFlags)

constexpr AccessCaps acr_flags_to_access_caps (AcrFlags f) {
    return AC::Read | AccessCaps(u8(
         // Flip Readonly and Unaddressable
        (~f & (AcrFlags::Readonly | AcrFlags::Unaddressable)) |
         // Merge ~Unaddressable into ChildrenAddressable
        ((f | (~f) << 4) & AcrFlags::ChildrenAddressable)
    ));
}

constexpr TreeFlags acr_flags_to_tree_flags (AcrFlags f) {
    return TreeFlags(u8(f) >> 8);
}

 // These belong on AttrDcr and ElemDcr, but we're putting them with the
 // accessor flags to save space.
enum class AttrFlags : u8 {
     // If this is set, the attr doesn't need to be present when doing
     // the from_tree operation.  There's no support for default values here;
     // if an attr wants a default value, set it in the class's default
     // constructor.  This is allowed on elems, but all optional elems must
     // follow all non-optional elems (allowing optional elems in the middle
     // would change the apparent index of later required elems, which would
     // be confusing).
    Optional = 0x1,
     // If this is set, the attrs of this attr will be included in the
     // serialization of this item and available through calls to attr().  This
     // is not currently supported on elems.
    Collapse = 0x2,
     // If this is set, this item will be able to be upcasted to the type of the
     // attr/elem if it is addressable.
    Castable = 0x4,
     // If this is set, the attr will not be serialized in to_tree.
    Invisible = 0x8,
     // If this is set, the attr will not be deserialized in from_tree.
    Ignored = 0x10,
     // If this is set, there is a tree 16 bytes before the attr's key, which is
     // the default value of the attr.
    HasDefault = 0x20,
     // If this is set, map an empty array to the attribute being missing from
     // the object, and an array of one element to the attribute being present
     // with that element as its value.
    CollapseOptional = 0x40,
};
DECLARE_ENUM_BITWISE_OPERATORS(AttrFlags)

struct Accessor;
 // This is the "virtual function" that accessors use
using AccessFunc = void(const Accessor*, Mu&, AccessCB, AccessCaps);

 // Arrange these in rough order of commonality for cachiness
enum class AcrForm : u8 {
    Identity,
    Reinterpret,
    Member,
    RefFunc,
    ConstantPtr,
    AnyRefFunc,
    AnyPtrFunc,
    PtrToAnyRef,
    Functive, // Miscellaneous functive accessor that doesn't need destructing
    Variable,
    Chain,
    ChainAttrFunc,
    ChainElemFunc,
    ChainDataFunc,
};
using AF = AcrForm;

 // Access function lookup table.  GCC's switch-statement jump tables have
 // issues with register allocation, so we're using our own.
void access_Identity (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Reinterpret (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Member (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_RefFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_ConstantPtr (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_AnyRefFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_AnyPtrFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_PtrToAnyRef (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Functive (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Variable (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_Chain (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_ChainAttrFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_ChainElemFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
void access_ChainDataFunc (const Accessor*, Mu&, AccessCB, AccessCaps);
constexpr AccessFunc* access_table [] = {
    access_Identity,
    access_Reinterpret,
    access_Member,
    access_RefFunc,
    access_ConstantPtr,
    access_AnyRefFunc,
    access_AnyPtrFunc,
    access_PtrToAnyRef,
    access_Functive,
    access_Variable,
    access_Chain,
    access_ChainAttrFunc,
    access_ChainElemFunc,
    access_ChainDataFunc,
};

 // The base class for all accessors.  Try to keep this small.
struct Accessor {
     // If ref_count is 0, this is a constexpr accessor and it can't be
     // modified.  Yes, this does mean that if an accessor accumulates enough
     // references to overflow the count it won't be deleted.  I doubt you'll
     // care.  (Usually refcounts are marked mutable, but that's illegal in
     // constexpr classes.)  Note also that the refcount starts at 1, so when
     // constructing an AnyRef or a ChainAcr with a new Accessor*, don't call
     // inc() on it.
    u32 ref_count = 1;
    AcrForm form;
    AccessCaps caps;
    TreeFlags tree_flags;
     // These belong on AttrDcr and ElemDcr but we're storing them here to
     // save space.
    AttrFlags attr_flags = {};

     // Constructor for ad-hoc accessors.  The tree_flags and attr_flags should
     // never be used on this.
    explicit constexpr Accessor (AcrForm f, AccessCaps c) :
        form(f), caps(c), tree_flags()
    { }

    explicit constexpr Accessor (AcrForm s, AcrFlags f) :
        form(s),
        caps(acr_flags_to_access_caps(f)),
        tree_flags(acr_flags_to_tree_flags(f))
    { }

    void access (AccessCaps mode, Mu& from, AccessCB cb) const {
        expect(mode <= caps);
        access_table[u8(form)](this, from, cb, mode);
    }

    void read (Mu& from, AccessCB cb) const {
        access(AC::Read, from, cb);
    }
    void write (Mu& from, AccessCB cb) const {
        access(AC::Write, from, cb);
    }
    void modify (Mu& from, AccessCB cb) const {
        access(AC::Modify, from, cb);
    }

     // This doesn't really feel like it belongs here but it's too convenient
    AnyPtr address (Mu& from) const;

    void inc () const {
         // Unlikely because most ACRs are constexpr.
        if (ref_count) [[unlikely]] {
            const_cast<u32&>(ref_count)++;
        }
    }
    void do_dec () noexcept;
    constexpr void dec () const {
        if (ref_count) [[unlikely]] const_cast<Accessor*>(this)->do_dec();
    }
};

 // Yes Accessors are comparable!  Two Accessors are the same if they come from
 // the same place in the same AYU_DESCRIBE block, or if they're dynamically
 // generated from the same inputs.  Access capabilities and other flags are
 // IGNORED when comparing accessors for equality.
bool operator== (const Accessor&, const Accessor&);
usize hash_acr (const Accessor&);

template <class Acr>
constexpr Acr constexpr_acr (Acr a) {
    a.ref_count = 0;
    return a;
}

} // ayu::in

template <>
struct std::hash<ayu::in::Accessor> {
    size_t operator () (const ayu::in::Accessor& acr) const {
        return ayu::in::hash_acr(acr);
    }
};
