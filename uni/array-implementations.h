 // This is here if you want to access the underlying implementation of the
 // array classes.  These structs have no behavior associated with them, so if
 // you want to deal with them, you have to manage refcounts and such yourself.
#pragma once
#include "common.h"

namespace uni {
inline namespace arrays {

///// ARRAY TYPES 
enum class ArrayClass {
    AnyA,
    AnyS,
    StaticA,
    StaticS,
    SharedA,
    SharedS,
    UniqueA,
    UniqueS,
    SliceA,
    SliceS
};

///// ARRAY IMPLEMENTATIONS

template <ArrayClass, class>
struct ArrayImplementation;

template <class T>
struct ArrayImplementation<ArrayClass::AnyA, T> {
     // The first bit is owned, the rest is size (shifted left by 1).
     // owned = sizex2_with_owned & 1
     // size = sizex2_with_owned >> 1
     // We are not using bitfields because they are not optimized very well.
    uint32 sizex2_with_owned;
    T* data;
};

template <class T>
struct ArrayImplementation<ArrayClass::AnyS, T> {
    uint32 sizex2_with_owned;
    T* data;
};

template <class T>
struct ArrayImplementation<ArrayClass::SharedA, T> {
    uint32 size;
    T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::SharedS, T> {
    uint32 size;
    T* data;
};

template <class T>
struct ArrayImplementation<ArrayClass::UniqueA, T> {
    uint32 size;
    T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::UniqueS, T> {
    uint32 size;
    T* data;
};

template <class T>
struct ArrayImplementation<ArrayClass::StaticA, T> {
    usize size;
    const T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::StaticS, T> {
    usize size;
    const T* data;
};

template <class T>
struct ArrayImplementation<ArrayClass::SliceA, T> {
    usize size;
    const T* data;
};
template <class T>
struct ArrayImplementation<ArrayClass::SliceS, T> {
    usize size;
    const T* data;
};

} // arrays
} // uni
