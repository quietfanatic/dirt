 // This is here if you want to access the underlying implementation of the
 // array classes.  These structs have no behavior associated with them, so if
 // you want to deal with them, you have to manage refcounts and such yourself.
#pragma once
#include "common.h"

namespace uni {
inline namespace arrays {

///// ARRAY TYPES

namespace ArrayClass {
    struct Defaults {
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
    struct AnyArray : Defaults {
        static constexpr bool is_Any = true;
        static constexpr bool supports_share = true;
        static constexpr bool supports_owned = true;
        static constexpr bool supports_static = true;
    };
    struct AnyString : AnyArray {
        static constexpr bool is_String = true;
    };
    struct UniqueArray : Defaults {
        static constexpr bool is_Unique = true;
        static constexpr bool supports_owned = true;
        static constexpr bool mut_default = true;
    };
    struct UniqueString : UniqueArray {
        static constexpr bool is_String = true;
    };
    struct StaticArray : Defaults {
        static constexpr bool is_Static = true;
        static constexpr bool supports_static = true;
        static constexpr bool trivially_copyable = true;
    };
    struct StaticString : StaticArray {
        static constexpr bool is_String = true;
    };
    struct Slice : Defaults {
        static constexpr bool is_Slice = true;
        static constexpr bool trivially_copyable = true;
    };
    struct Str : Slice {
        static constexpr bool is_String = true;
    };
    struct MutSlice : Defaults {
        static constexpr bool is_MutSlice = true;
        static constexpr bool trivially_copyable = true;
        static constexpr bool mut_default = true;
    };
    struct MutStr : MutSlice {
        static constexpr bool is_String = true;
    };
}

///// ARRAY IMPLEMENTATIONS

template <class, class>
struct ArrayImplementation;

template <class T>
struct ArrayImplementation<ArrayClass::AnyArray, T> {
     // The lowest bit is owned, the rest is size (shifted left by 1).
     // owned = sizex2_with_owned & 1
     // size = sizex2_with_owned >> 1
     // We are not using bitfields because they are not optimized very well.
    uint32 sizex2_with_owned;
    T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::AnyString, T> {
    uint32 sizex2_with_owned;
    T* data;
};


template <class T>
struct ArrayImplementation<ArrayClass::UniqueArray, T> {
    uint32 size;
    T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::UniqueString, T> {
    uint32 size;
    T* data;
};

template <class T>
struct ArrayImplementation<ArrayClass::StaticArray, T> {
    usize size;
    const T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::StaticString, T> {
    usize size;
    const T* data;
};

template <class T>
struct ArrayImplementation<ArrayClass::Slice, T> {
    usize size;
    const T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::Str, T> {
    usize size;
    const T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::MutSlice, T> {
    usize size;
    T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::MutStr, T> {
    usize size;
    T* data;
};

} // arrays
} // uni
