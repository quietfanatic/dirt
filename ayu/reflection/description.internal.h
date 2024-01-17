 // A Description contains all the information that's necessary for all types.
 // Per-type information is in descriptors-internal.h.

#pragma once

#include <typeinfo>

#include "../common.h"

namespace ayu::in {

 // To compare addresses of disparate types, we need to cast them to a common
 // type.  reinterpret_cast is not allowed in constexprs (and in recent gcc
 // versions, neither are C-style casts), but static_cast to a common base
 // type is.
struct ComparableAddress { };
static_assert(sizeof(ComparableAddress) == 1);

struct Description : ComparableAddress {
#ifdef AYU_STORE_TYPE_INFO
    const std::type_info* cpp_type = null;
#endif
    uint32 cpp_size = 0;
    uint32 cpp_align = 0;
     // Empty if the name needs to be dynamically generated.
    StaticString name;

     // Do some property calculations ahead of time
    enum Flags {
        PREFER_ARRAY = 1 << 0,
        PREFER_OBJECT = 1 << 1,
        PREFERENCE = PREFER_ARRAY | PREFER_OBJECT,
         // Can select some faster algorithms when this is false.
        SHOULD_REBUILD_OBJECT = 1 << 2,
         // Faster values() processing
        ALL_VALUES_STRINGS = 1 << 3,
         // These are actually only used at compile time, but we have plenty of
         // bits available so it doesn't hurt to leave them around.
        CUSTOM_DEFAULT_CONSTRUCT = 1 << 4,
        CUSTOM_DESTROY = 1 << 5,
    };
    uint16 flags = 0;

    uint16 name_offset = 0;
    uint16 to_tree_offset = 0;
    uint16 from_tree_offset = 0;
    uint16 swizzle_offset = 0;
    uint16 init_offset = 0;
    uint16 values_offset = 0;
    uint16 attrs_offset = 0;
    uint16 elems_offset = 0;
    uint16 keys_offset = 0;
    uint16 computed_attrs_offset = 0;
    uint16 length_offset = 0;
    uint16 computed_elems_offset = 0;
    uint16 contiguous_elems_offset = 0;
    uint16 delegate_offset = 0;
};

} // namespace ayu::in

 // I was going to use ayu::desc here but using a nested namespace seems to
 // cause weird errors in some situations.  Besides, having the namespace nested
 // in ayu:: automatically makes names in ayu:: visible, which may not be
 // desired.
namespace ayu_desc {
    template <class T>
    struct _AYU_Describe {
         // Declare this but don't define it.  It will be defined in a
         // specialization of this template, which may be in a different
         // translation unit.  Apparently nobody knows whether that's legal or
         // not, but it works as long as the compiler uses the same mangled
         // names for the specialization as the prototype.
         //
         // It'd be nice to have this be the description itself instead of a
         // pointer to it, but unfortunately you can't have a global cross a
         // compilation boundary unless its type is fully known.
        static const ayu::in::Description* const _ayu_description;
    };
}

namespace ayu::in {
    const Description* register_description (const Description*) noexcept;
#ifdef AYU_STORE_TYPE_INFO
    const Description* get_description_for_type_info (const std::type_info&) noexcept;
    const Description* need_description_for_type_info (const std::type_info&);
#endif
    const Description* get_description_for_name (Str) noexcept;
    const Description* need_description_for_name (Str);

    StaticString get_description_name (const Description*);

    UniqueString get_demangled_name (const std::type_info&) noexcept;

    template <class T> requires (!std::is_reference_v<T>)
    constexpr const Description* const* get_indirect_description () {
        return &ayu_desc::_AYU_Describe<std::remove_cv_t<T>>::_ayu_description;
    }

    template <class T> requires (!std::is_reference_v<T>)
    const Description* get_description_for_cpp_type () {
        return ayu_desc::_AYU_Describe<std::remove_cv_t<T>>::_ayu_description;
    }
}

