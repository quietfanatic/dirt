#pragma once
#include "description.internal.h"

namespace ayu::in {

struct TypeInfo {
    const ayu::in::Description* description;
};

 // I was going to use ayu::desc here but using a nested namespace seems to
 // cause weird errors in some situations.  Besides, having the namespace nested
 // in ayu:: automatically makes names in ayu:: visible, which may not be
 // desired.
} namespace ayu_desc {
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
         // compilation boundary unless its type is fully known.  At least not
         // without some very illegal tricks and non-portable linker flags...
        static const ayu::in::TypeInfo _ayu_type_info;
    };
} namespace ayu::in {

void register_type (TypeInfo*) noexcept;
const TypeInfo* get_type_for_name (Str) noexcept;
const TypeInfo* require_type_for_name (Str);

StaticString get_type_name (const TypeInfo*) noexcept;

UniqueString get_demangled_name (const std::type_info&) noexcept;

template <class T> requires (!std::is_reference_v<T>)
constexpr const TypeInfo* get_type_info () {
    return &ayu_desc::_AYU_Describe<std::remove_cv_t<T>>::_ayu_type_info;
}

} // ayu::in
