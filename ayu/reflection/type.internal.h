#pragma once

#include "access.internal1.h"

 // Don't bother putting these in a namespace, since the namespace leaks when
 // you're overriding them anyway.  Just use C-style name prefixes.
template <class T> struct AYU_Description;
template <class T> struct AYU_Describe {
     // Declare this but don't define it.  It will be defined in a
     // specialization of this template, which may be in a different translation
     // unit.  Apparently nobody knows whether that's legal or not, but it works
     // as long as the compiler uses the same mangled names for the
     // specialization as the prototype.
    static const AYU_Description<T> AYU_description;
};

namespace ayu::in {

 // To compare addresses of disparate types, we need to cast them to a common
 // type.  reinterpret_cast is not allowed in constexprs (and in recent gcc
 // versions, neither are C-style casts), but static_cast to a common base
 // type is.
struct ComparableAddress { };
static_assert(sizeof(ComparableAddress) == 1);

using NameFunc = AnyString();

enum class DescFlags : u8 {
    PreferArray = 1 << 0,
    PreferObject = 1 << 1,
    Preference = PreferArray | PreferObject,
     // Select between union members
    NameComputed = 1 << 2,
    ElemsContiguous = 1 << 3,
     // Can select some faster algorithms when this is false.
    AttrsNeedRebuild = 1 << 4,
     // Faster values() processing
    ValuesAllStrings = 1 << 5,
};
DECLARE_ENUM_BITWISE_OPERATORS(DescFlags)

enum class TypeFlags : u8 {
    NoRefsToChildren = 1 << 0,
    NoRefsFromChildren = 1 << 1,
};
DECLARE_ENUM_BITWISE_OPERATORS(TypeFlags)

struct IdentityAccessor : Accessor {
    constexpr IdentityAccessor () :
        Accessor(AcrForm::Identity, AccessCaps::AllowEverything)
    { ref_count = 0; }
};

struct DescriptionHeader : IdentityAccessor, ComparableAddress {
    u32 cpp_size = 0;
    u32 cpp_align = 0;
    union {
        StaticString name;
        struct {
            StaticString* cache;
            NameFunc* f;
        } computed_name;
    };

    DescFlags flags = {};
    TypeFlags type_flags = {};

    u16 to_tree_offset = 0;
    u16 from_tree_offset = 0;
    u16 before_from_tree_offset = 0;
    u16 swizzle_offset = 0;
    u16 init_offset = 0;
    u16 values_offset = 0;
    u16 keys_offset = 0;
    union {
        u16 attrs_offset = 0; // keys_offset == 0
        u16 computed_attrs_offset; // keys_offset != 0
    };
    u16 length_offset = 0;
    union {
        u16 elems_offset = 0; // length_offset == 0
         // length_offset != 0 && !ElemsContiguous
        u16 computed_elems_offset;
         // length_offset != 0 && ElemsContiguous
        u16 contiguous_elems_offset;
    };
    u16 delegate_offset = 0;

    StaticString get_name () const noexcept;
};

void register_description (const void*) noexcept;

const DescriptionHeader* require_type_with_name (Str name);

} // ayu::in
