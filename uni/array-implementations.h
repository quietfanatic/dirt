 // This is here if you want to access the underlying implementation of the
 // arrays.  These structs are raw data with no behavior, so if you want to deal
 // with them, you have to manage refcounts and such yourself.
#pragma once
#include "common.h"

namespace uni {

///// ARRAY CLASSES (in the mathematical sense of classification)

struct ArrayClassDefaults {
    static constexpr bool is_String = false;
    static constexpr bool is_Any = false;
    static constexpr bool is_Unique = false;
    static constexpr bool is_Static = false;
    static constexpr bool is_Slice = false;
    static constexpr bool is_MutSlice = false;
    static constexpr bool supports_share = false;
    static constexpr bool supports_owned = false;
    static constexpr bool supports_static = false;
    static constexpr bool trivially_copyable = false;
    static constexpr bool mut_default = false;
};
struct AnyArrayClass : ArrayClassDefaults {
    static constexpr bool is_Any = true;
    static constexpr bool supports_share = true;
    static constexpr bool supports_owned = true;
    static constexpr bool supports_static = true;
};
struct AnyStringClass : AnyArrayClass {
    static constexpr bool is_String = true;
};
struct UniqueArrayClass : ArrayClassDefaults {
    static constexpr bool is_Unique = true;
    static constexpr bool supports_owned = true;
    static constexpr bool mut_default = true;
};
struct UniqueStringClass : UniqueArrayClass {
    static constexpr bool is_String = true;
};
struct StaticArrayClass : ArrayClassDefaults {
    static constexpr bool is_Static = true;
    static constexpr bool supports_static = true;
    static constexpr bool trivially_copyable = true;
};
struct StaticStringClass : StaticArrayClass {
    static constexpr bool is_String = true;
};
struct SliceClass : ArrayClassDefaults {
    static constexpr bool is_Slice = true;
    static constexpr bool trivially_copyable = true;
};
struct StrClass : SliceClass {
    static constexpr bool is_String = true;
};
struct MutSliceClass : ArrayClassDefaults {
    static constexpr bool is_MutSlice = true;
    static constexpr bool trivially_copyable = true;
    static constexpr bool mut_default = true;
};
struct MutStrClass : MutSliceClass {
    static constexpr bool is_String = true;
};

///// ARRAY IMPLEMENTATIONS

template <class, class>
struct ArrayImpl;

template <class ac, class T> requires (ac::is_Any)
struct ArrayImpl<ac, T> {
     // The lowest bit is owned, the rest is size (shifted left by 1).
     // owned = sizex2_with_owned & 1
     // size = sizex2_with_owned >> 1
     // We are not using bitfields because they are not optimized very well.
    usize sizex2_with_owned;
    T* data;
};

template <class ac, class T> requires (ac::mut_default)
struct ArrayImpl<ac, T> {
    usize size;
    T* data;
};

template <class ac, class T> requires (!ac::is_Any && !ac::mut_default)
struct ArrayImpl<ac, T> {
    usize size;
    const T* data;
};

} // uni
