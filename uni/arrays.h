 // Arrays that can be shared (ref-counted) or static
 //
 // This header provides a constellation of array and string classes that share
 // a common interface and differ by ownership model.  They're largely
 // compatible with STL containers, but not quite a drop-in replacement.
 //
 // COPY-ON-WRITE
 // The AnyArray and AnyString classes have copy-on-write behavior when you try
 // to modify them.  By default simple access operations like begin(), end(),
 // operator[], and at() do not trigger a copy-on-write and instead return const
 // references.  To trigger a copy-on-write use mut_begin(), mut_end(),
 // mut_get(), and mut_at().  For simplicity, AnyArray and AnyString can only be
 // used with element types that have a copy constructor.  To work with move-
 // only element types, use UniqueArray.
 //
 // STATIC STRING OPTIMIZATION
 // Not to be confused with Small String Optimization.  AnyArray and AnyString
 // can refer to a static string (a string which will stay alive for the
 // duration of the program, or at least the foreseeable future).  This allows
 // them to be created and passed around with no allocation cost.  For
 // optimization, both StaticString and AnyString expect that fixed-size const
 // char[] (but not non-const char[]) is a string literal.  If you use
 // dynamically-allocated fixed-size raw char arrays, you might run into some
 // use-after-free surprises.  I think this should be rare to nonexistent in
 // practice.
 //
 // THREAD-SAFETY
 // AnyArray and AnyString use reference counting which is not threadsafe.  To
 // pass arrays and strings between threads you should use UniqueArray and
 // UniqueString.
 //
 // EXCEPTION-SAFETY
 // None of these classes generate their own exceptions.  If an out-of-bounds
 // index or a too-large-for-capacity problem occurs, the program will be
 // terminated.  If an element type throws an exception in its default
 // constructor, copy constructor, or copy assignment operator, the array method
 // provides a mostly-strong exception guarantee (except for multi-element
 // insert): All semantic state will be rewound to before the method was called,
 // but non-semantic state (such as capacity and whether a buffer is shared) may
 // be altered.  However, if an element type throws an exception in its move
 // constructor, move assignment constructor, or destructor, undefined behavior
 // will occur (hopefully after a debug-only assertion).
#pragma once

#include <filesystem> // for conversion to path
#include <functional> // std::hash
#include <iterator>
#include "array-implementations.h"
#include "assertions.h"
#include "buffers.h"
#include "common.h"
#include "copy-ref.h"

namespace uni {
inline namespace strings { namespace in { struct Cats; } }
inline namespace arrays {

///// THIS HEADER PROVIDES

 // The base interface for all array classes.
template <ArrayClass ac, class T>
struct ArrayInterface;

 // A generic dynamically-sized array class which can either own shared
 // (refcounted) data or reference static data.  Has semi-implicit
 // copy-on-write behavior (at no cost if you don't use it).
template <class T>
using AnyArray = ArrayInterface<ArrayClass::AnyA, T>;

 // An array that can only reference shared data.  There isn't much reason to
 // use this instead of AnyArray, but it's here as an intermediate between AnyArray
 // and UniqueArray.  This should probably be renamed because its name suggests
 // it might be have shared mutability, but it does not.
template <class T>
using SharedArray = ArrayInterface<ArrayClass::SharedA, T>;

 // An array that guarantees unique ownership, allowing mutation without
 // copy-on-write.  This has the same role as std::vector.
template <class T>
using UniqueArray = ArrayInterface<ArrayClass::UniqueA, T>;

 // An array that can only reference static data (or at least data that outlives
 // it).  The data cannot be modified.  The difference between this and Slice is
 // that an AnyArray can be constructed from a StaticArray without allocating a new
 // buffer.
template <class T>
using StaticArray = ArrayInterface<ArrayClass::StaticA, T>;

 // A non-owning view of contiguous elements.  This has the same role as
 // std::span (but without fixed extents).
template <class T>
using Slice = ArrayInterface<ArrayClass::SliceA, T>;

 // The string types are almost exactly the same as the equivalent array types,
 // but they have slightly different rules for array construction; they can be
 // constructed from a const T*, which is taken to be a C-style NUL-terminated
 // string, and when constructing from a C array, they will chop off the final
 // NUL element.  Note that by default these strings are not NUL-terminated.  To
 // get a NUL-terminated string out, either explicitly NUL-terminate them or use
 // c_str().
template <class T>
using GenericAnyString = ArrayInterface<ArrayClass::AnyS, T>;
template <class T>
using GenericSharedString = ArrayInterface<ArrayClass::SharedS, T>;
template <class T>
using GenericUniqueString = ArrayInterface<ArrayClass::UniqueS, T>;
template <class T>
using GenericStaticString = ArrayInterface<ArrayClass::StaticS, T>;
template <class T>
using GenericStr = ArrayInterface<ArrayClass::SliceS, T>;

using AnyString = GenericAnyString<char>;
using SharedString = GenericSharedString<char>;
using UniqueString = GenericUniqueString<char>;
using StaticString = GenericStaticString<char>;
using Str = GenericStr<char>;

using AnyString16 = GenericAnyString<char16>;
using SharedString16 = GenericSharedString<char16>;
using UniqueString16 = GenericUniqueString<char16>;
using StaticString16 = GenericStaticString<char16>;
using Str16 = GenericStr<char16>;

using AnyString32 = GenericAnyString<char32>;
using SharedString32 = GenericSharedString<char32>;
using UniqueString32 = GenericUniqueString<char32>;
using StaticString32 = GenericStaticString<char32>;
using Str32 = GenericStr<char32>;

///// ARRAYLIKE CONCEPTS
 // A general concept for array-like types.
template <class A>
concept ArrayLike = requires (A a) { *a.data(); usize(a.size()); };
 // An array-like type that is NOT one of the classes defined here.
template <class A>
concept OtherArrayLike = ArrayLike<A> && !requires { A::is_ArrayInterface; };
 // Array-like for a specific element type.
template <class A, class T>
concept ArrayLikeFor = ArrayLike<A> && std::is_same_v<
    std::remove_cvref_t<decltype(*std::declval<A>().data())>, T
>;
 // Foreign array-like class with specific element type.
template <class A, class T>
concept OtherArrayLikeFor = ArrayLikeFor<A, T> &&
    !requires { A::is_ArrayInterface; };

///// UTILITY CONCEPTS AND STUFF
 // For these concepts, we are deliberately not constraining return types.  Our
 // policy for concepts is to use them for overload resolution, not for semantic
 // guarantees.  And since the requires() clause of a function is part of its
 // interface, we want to keep it as simple as possible.  The STL iterator
 // concepts library is WAY too complicated for this purpose.

 // An ArrayIterator is just anything that can be dereferenced and incremented.
template <class I>
concept ArrayIterator = requires (I i) { *i; ++i; };
 // An ArrayIteratorFor<T> is an ArrayIterator that when dereferenced provides a
 // T.
template <class I, class T>
concept ArrayIteratorFor = ArrayIterator<I> && std::is_same_v<
    std::remove_cvref_t<decltype(*std::declval<I>())>,
    std::remove_cv_t<T>
>;
 // An ArraySentinelFor<Begin> is an iterator that can be compared to Begin with
 // !=.  That is its only requirement; it doesn't have to be dereferencable.
template <class End, class Begin>
concept ArraySentinelFor = requires (Begin b, End e) { b != e; };
 // An ArrayContiguousIterator is one that guarantees that the elements are
 // contiguous in memory like a C array.
 // (We're reluctantly delegating to STL for contiguousity because it cannot be
 // verified at compile time, so the STL just explicitly specifies it for
 // iterators that model it.)
template <class I>
concept ArrayContiguousIterator = std::is_base_of_v<
    std::contiguous_iterator_tag,
    typename std::iterator_traits<std::remove_cvref_t<I>>::iterator_concept
>;
 // An ArrayContiguousIteratorFor<T> can be martialled into a T*.
template <class I, class T>
concept ArrayContiguousIteratorFor =
    ArrayContiguousIterator<I> && std::is_same_v<
        std::remove_cvref_t<decltype(*std::declval<I>())>,
        std::remove_cv_t<T>
    >;
 // An ArrayForwardIterator is one that can be copied, meaning that the array
 // can be walked through multiple times.
template <class I>
concept ArrayForwardIterator = ArrayIterator<I> && std::is_copy_constructible_v<I>;

 // A tag-like type representing a span of uninitialized data
struct Uninitialized { usize size; };
 // Requests constructing with the given capacity but size 0
struct Capacity { usize cap; };

///// ARRAY INTERFACE
// The shared interface for all the array classes

template <ArrayClass ac, class T>
struct ArrayInterface {
    static_assert(
        alignof(T) <= 8, "Arrays with elements that have align > 8 are NYI."
    );
    using Self = ArrayInterface<ac, T>;
    using Impl = ArrayImplementation<ac, T>;
  private:
    Impl impl;
    template <ArrayClass, class>
    friend struct ArrayInterface;
  public:

    ///// PSEUDO-CONCEPTS
    static constexpr bool is_ArrayInterface = true;
    static constexpr bool is_String = ac == ArrayClass::AnyS ||
                                      ac == ArrayClass::SharedS ||
                                      ac == ArrayClass::UniqueS ||
                                      ac == ArrayClass::StaticS ||
                                      ac == ArrayClass::SliceS;
    static constexpr bool is_Any = ac == ArrayClass::AnyA ||
                                   ac == ArrayClass::AnyS;
    static constexpr bool is_Shared = ac == ArrayClass::SharedA ||
                                      ac == ArrayClass::SharedS;
    static constexpr bool is_Unique = ac == ArrayClass::UniqueA ||
                                      ac == ArrayClass::UniqueS;
    static constexpr bool is_Static = ac == ArrayClass::StaticA ||
                                      ac == ArrayClass::StaticS;
    static constexpr bool is_Slice = ac == ArrayClass::SliceA ||
                                     ac == ArrayClass::SliceS;
    static constexpr bool supports_share = is_Any || is_Shared;
    static constexpr bool supports_owned = supports_share || is_Unique;
    static constexpr bool supports_static = is_Any || is_Static;
    static constexpr bool trivially_copyable = is_Static || is_Slice;

    static_assert(
        !(supports_share && !std::is_copy_constructible_v<T>),
        "Cannot have a copy-on-write array/string with a non-copyable element type."
    );

    ///// TYPEDEFS
    using value_type = T;
    using size_type = usize;
    using difference_type = isize;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = std::conditional_t<is_Unique, T*, const T*>;
    using const_iterator = const T*;
    using mut_iterator = std::conditional_t<supports_owned, T*, void>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const T*>;
    using mut_reverse_iterator =
        std::conditional_t<supports_owned, std::reverse_iterator<T*>, void>;

    using SelfSlice = std::conditional_t<
        is_String, ArrayInterface<ArrayClass::SliceS, T>, Slice<T>
    >;

    ///// CONSTRUCTION
     // Default construct, makes an empty array.
    ALWAYS_INLINE constexpr
    ArrayInterface () : impl{} { }

     // Move construct.
    ALWAYS_INLINE constexpr
    ArrayInterface (ArrayInterface&& o) requires (!trivially_copyable) {
        impl = o.impl;
        o.impl = {};
    }
     // We need to default the move constructor too, not just the copy
     // constructor, or the Itanium C++ ABI won't pass this in registers.
    ArrayInterface (ArrayInterface&& o) requires (trivially_copyable) = default;
     // Move conversion.  Tries to make the moved-to array have the same
     // ownership mode as the moved-from array, and if that isn't supported,
     // copies the buffer.  Although move conversion will never fail for
     // copyable element types, it's disabled for StaticArray and Slice, and the
     // copy constructor from AnyArray to StaticArray can fail.
    template <ArrayClass ac2> ALWAYS_INLINE constexpr
    ArrayInterface (ArrayInterface<ac2, T>&& o) requires (
        !trivially_copyable && !o.trivially_copyable
    ) {
        if (supports_share || o.unique()) {
            set_owned(o.impl.data, o.size());
            o.impl = {};
        }
        else if constexpr (std::is_copy_constructible_v<T>) {
             // In cases where a copy may or may not be required, noinline the
             // copy to get the slow path out of the way.
            set_copy_noinline(o.impl.data, o.size());
        }
        else never();
    }

     // Copy construct.  Always copies the buffer for UniqueArray, never copies
     // the buffer for other array classes.
    ALWAYS_INLINE constexpr
    ArrayInterface (const ArrayInterface& o) requires (!trivially_copyable) {
        if constexpr (is_Unique) {
            set_copy(o.impl.data, o.size());
        }
        else if constexpr (supports_share) {
            impl = o.impl;
            add_ref();
        }
        else {
            set_unowned(o.impl.data, o.size());
        }
    }
     // Copy constructor is defaulted for StaticArray and Slice so that they can
     // have the is_trivially_copy_constructible trait.
    ArrayInterface (const ArrayInterface&) requires (trivially_copyable)
        = default;

     // Copy convert.  Tries to make this array have the same ownership mode as
     // the passed array, and if that isn't supported, tries to copy, and if
     // that isn't supported, borrows.  This is explicit for converting to
     // StaticArray, because it's not guaranteed that the data is actually
     // static.
    template <ArrayClass ac2> ALWAYS_INLINE constexpr
    explicit(is_Static && !ArrayInterface<ac2, T>::is_Static)
    ArrayInterface (const ArrayInterface<ac2, T>& o) {
        if constexpr (supports_share && o.supports_share) {
            if (o.owned()) {
                set_owned(o.impl.data, o.size());
                add_ref();
                return;
            }
        }
        if constexpr (supports_static && o.supports_static) {
            set_unowned(o.impl.data, o.size());
        }
        else if constexpr (supports_owned) {
            set_copy(o.impl.data, o.size());
        }
        else {
            set_unowned(o.impl.data, o.size());
        }
    }

     // Copy construction from other array-like types.  Explicit except for
     // Slice/Str.
    template <OtherArrayLike O> ALWAYS_INLINE constexpr
    explicit(is_Static || !(is_Slice && ArrayLikeFor<O, T>))
    ArrayInterface (const O& o) requires (!is_Static) {
        if constexpr (supports_owned) {
            set_copy(o.data(), o.size());
        }
        else {
            static_assert(ArrayLikeFor<O, T>,
                "Cannot construct borrowed array from array-like type if its "
                "element type does not match exactly."
            );
            static_assert(ArrayContiguousIterator<decltype(o.data())>,
                "Cannot construct borrowed from array-like type if its data() "
                "returns a non-contiguous iterator."
            );
            set_unowned(o.data(), o.size());
        }
    }

     // SPECIAL CASE allow construcing a Str from a char8 OtherArrayLike
    template <OtherArrayLikeFor<char8_t> O> ALWAYS_INLINE constexpr explicit
    ArrayInterface (const O& o) requires (
        is_String && is_Slice && std::is_same_v<T, char>
    ) {
        static_assert(ArrayContiguousIterator<decltype(o.data())>,
            "Cannot construct Slice from array-like type if its data() "
            "returns a non-contiguous iterator."
        );
        set_unowned(reinterpret_cast<const char*>(o.data()), o.size());
    }

     // Constructing from const T* is only allowed for String classes.  It will
     // take elements until the first one that boolifies to false.
    template <class O> constexpr
    ArrayInterface (O o) requires (
         // Some awkward requiresing to avoid decaying raw arrays.
        is_String && std::is_pointer_v<std::remove_cvref_t<O>> &&
        std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<O>>, T>
    ) {
        const T* p = o;
        expect(p);
        usize s = 0;
        while (p[s]) ++s;
        if constexpr (supports_owned) {
            set_copy(p, s);
        }
        else {
            set_unowned(p, s);
        }
    }

     // Construction from a raw C array.  For *Array, this always copies if
     // possible and borrows otherwise.
    template <class T2, usize len> ALWAYS_INLINE constexpr
    explicit(!std::is_same_v<T2, T>)
    ArrayInterface (const T2(& o )[len]) requires (!is_String) {
        if constexpr (supports_owned) {
            set_copy(o, len);
        }
        else {
            static_assert(std::is_same_v<T2, T>,
                "Cannot construct borrowed array from raw C array if its "
                "element type does not match exactly."
            );
            set_unowned(o, len);
        }
    }
     // For *String, the behavior differs based on whether the C array is const
     // or not.  A const array is assumed to be a string literal, which should
     // be safe to borrow from.
    template <class T2, usize len> ALWAYS_INLINE constexpr
    explicit(!std::is_same_v<T2, T>)
    ArrayInterface (const T2(& o )[len]) requires (is_String) {
         // String literals should always be NUL-terminated (and the NUL is
         // included in len).
        static_assert(len > 0,
            "Cannot construct borrowed string from raw C array of length 0 "
            "(it's assumed to be a NUL-terminated string literal, but there "
            "isn't room for a NUL terminator"
        );
        expect(!o[len-1]);
        if constexpr (supports_static && std::is_same_v<T2, T>) {
            set_unowned(o, len-1);
        }
        else if constexpr (supports_owned) {
            set_copy(o, len-1);
        }
        else {
            static_assert(std::is_same_v<T2, T>,
                "Cannot construct borrowed string from raw C array if its "
                "element type does not match exactly."
            );
            set_unowned(o, len-1);
        }
    }
     // A non-const C array behaves like the array version, and does NOT check
     // for NULs.
    template <class T2, usize len> ALWAYS_INLINE constexpr
    explicit(!std::is_same_v<T2, T>)
    ArrayInterface (T2(& o )[len]) requires (
        is_String && !std::is_const_v<T2>
    ) {
        if constexpr (supports_owned) {
            set_copy(o, len);
        }
        else {
            static_assert(std::is_same_v<T2, T>,
                "Cannot construct borrowed string from raw C array if its "
                "element type does not match exactly."
            );
            set_unowned(o, len);
        }
    }

     // Construct from a pointer(-like iterator) and a size
    template <ArrayIterator Ptr> ALWAYS_INLINE explicit constexpr
    ArrayInterface (Ptr p, usize s) {
        if constexpr (supports_owned) {
            set_copy(move(p), s);
        }
        else {
            static_assert(ArrayIteratorFor<Ptr, T>,
                "Cannot construct borrowed array from iterator with non-exact "
                "element type."
            );
            static_assert(ArrayContiguousIterator<Ptr>,
                "Cannot construct borrowed array from non-contiguous iterator."
            );
            set_unowned(std::to_address(move(p)), s);
        }
    }

     // Construct from a pair of iterators.
    template <ArrayIterator Begin, ArraySentinelFor<Begin> End>
    ALWAYS_INLINE explicit constexpr
    ArrayInterface (Begin b, End e) {
        if constexpr (supports_owned) {
            set_copy(move(b), move(e));
        }
        else {
            static_assert(ArrayIteratorFor<Begin, T>,
                "Cannot construct borrowed array from iterator with non-exact "
                "element type."
            );
            static_assert(ArrayContiguousIterator<Begin>,
                "Cannot construct borrowed array from non-contiguous iterator."
            );
            static_assert(requires { usize(e - b); },
                "Cannot construct borrowed array from iterator pair that "
                "doesn't support subtraction to get size."
            );
            set_unowned(std::to_address(move(b)), usize(e - b));
        }
    }

     // Construct an array with a number of default-constructed elements.  Only
     // available for owned classes.
    explicit
    ArrayInterface (usize s) requires (
        supports_owned && std::is_default_constructible_v<T>
    ) {
        if (!s) {
            impl = {}; return;
        }
        T* dat = SharableBuffer<T>::allocate(s);
        if (std::is_trivially_default_constructible_v<T>) {
            dat = (T*)std::memset((void*)dat, 0, s * sizeof(T));
        }
        else {
            usize i = 0;
            try {
                for (; i < s; ++i) {
                    new ((void*)&dat[i]) T();
                }
            }
            catch (...) {
                while (i > 0) {
                    impl.data[--i].~T();
                }
                SharableBuffer<T>::deallocate(dat);
                throw;
            }
        }
        set_unique(dat, s);
    }

     // This constructor constructs a repeating sequence of one element.  It's
     // only available for owned classes.
    explicit
    ArrayInterface (usize s, const T& v) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        if (!s) { impl = {}; return; }
        T* dat = SharableBuffer<T>::allocate(s);
        if constexpr (sizeof(T) == 1 &&
            std::is_trivially_copy_constructible_v<T>
        ) {
            dat = (T*)std::memset((void*)dat, v, s);
        }
        else {
            usize i = 0;
            try {
                for (; i < s; ++i) {
                    new ((void*)&dat[i]) T(v);
                }
            }
            catch (...) {
                while (i > 0) {
                    impl.data[--i].~T();
                }
                SharableBuffer<T>::deallocate(dat);
                throw;
            }
        }
        set_unique(dat, s);
    }

     // Construct with uninitialized data if the elements support that
    ALWAYS_INLINE explicit
    ArrayInterface (Uninitialized u) requires (
        supports_owned && std::is_trivially_default_constructible_v<T>
    ) {
        if (!u.size) { impl = {}; return; }
        set_unique(SharableBuffer<T>::allocate(u.size), u.size);
    }
     // Construct with specific capacity
    ALWAYS_INLINE explicit
    ArrayInterface (Capacity cap) requires (
        supports_owned
    ) {
        if (!cap.cap) { impl = {}; return; }
        set_unique(SharableBuffer<T>::allocate(cap.cap), 0);
    }

     // std::initializer_list isn't very good for owned types because it
     // requires copying all the items.
    ALWAYS_INLINE
    ArrayInterface (std::initializer_list<T> l) requires (supports_owned || is_Slice) {
        if constexpr (is_Slice) {
            set_unowned(std::data(l), std::size(l));
        }
        else set_copy(std::data(l), std::size(l));
    }
     // So use this named constructor instead.
    template <class... Args> ALWAYS_INLINE static
    ArrayInterface Make (Args&&... args) requires (supports_owned) {
        ArrayInterface r (Capacity(sizeof...(args)));
        (r.emplace_back_expect_capacity(std::forward<Args>(args)), ...);
        return r;
    }

    ///// ASSIGNMENT OPERATORS
    // These only take assignment from the exact same array class, relying on
    // implicit coercion for the others.
    // There is an opportunity for optimization that we aren't taking, that
    // being for copy assignment to unique strings to reuse the allocated buffer
    // instead of making a new one.  Currently I think this is more complicated
    // than it's worth.
    // Also, we should maybe detect when assigning a substr of self to self.

    ALWAYS_INLINE constexpr
    ArrayInterface& operator= (ArrayInterface&& o) requires (
        !trivially_copyable
    ) {
        if (&o == this) [[unlikely]] return *this;
        this->~ArrayInterface();
        return *new (this) ArrayInterface(move(o));
    }
    ALWAYS_INLINE constexpr
    ArrayInterface& operator= (const ArrayInterface& o) requires (
        !trivially_copyable
    ) {
        if (&o == this) [[unlikely]] return *this;
        return *this = ArrayInterface(o);
    }
    constexpr
    ArrayInterface& operator= (const ArrayInterface&) requires (
        trivially_copyable
    ) = default;

    ///// COERCION

     // Okay okay
    ALWAYS_INLINE constexpr
    operator std::basic_string_view<T> () const requires (is_String) {
        return std::basic_string_view<T>(impl.data, size());
    }
    ALWAYS_INLINE constexpr
    operator std::basic_string<T> () const requires (is_String) {
        return std::string(impl.data, size());
    }
     // Sigh
    ALWAYS_INLINE constexpr
    operator std::filesystem::path () const requires (
        is_String && sizeof(T) == sizeof(char) && requires (char c, T v) { c = v; }
    ) {
        return std::filesystem::path(
            reinterpret_cast<const char8_t*>(begin()),
            reinterpret_cast<const char8_t*>(end())
        );
    }

    ///// DESTRUCTOR
    ALWAYS_INLINE constexpr
    ~ArrayInterface () requires (!trivially_copyable) { remove_ref(); }
    ~ArrayInterface () requires (trivially_copyable) = default;

    ///// BYPASSING REFERENCE COUNTING
     // These manipulate the array without touching any reference counts,
     // constructors, or destructors, so obviously only do it if you know what
     // you're doing.
    ALWAYS_INLINE static constexpr
    Self UnsafeConstructOwned (T* d, usize s) {
        Self r;
        r.set_owned(d, s);
        return r;
    }
    ALWAYS_INLINE constexpr
    void unsafe_set_owned (T* d, usize s) {
#ifndef NDEBUG
        if (d) {
            expect(s <= max_size_);
            expect(s <= SharableBuffer<T>::header(d)->capacity);
            expect(SharableBuffer<T>::header(d)->capacity >= min_capacity);
            expect(SharableBuffer<T>::header(d)->capacity <= max_capacity);
        }
        else expect(!s);
#endif
        set_owned(d, s);
    }
    ALWAYS_INLINE constexpr
    void unsafe_set_unowned (T* d, usize s) {
#ifndef NDEBUG
        if (d) expect(s <= max_size_);
        else expect(!s);
#endif
        set_unowned(d, s);
    }
    ALWAYS_INLINE constexpr
    void unsafe_set_empty () {
        impl = {};
    }
    ALWAYS_INLINE constexpr
    void unsafe_set_size (usize s) {
        set_size(s);
    }

    ///// ACCESSORS

     // Gets whether this array is empty (size == 0)
    ALWAYS_INLINE constexpr
    bool empty () const { return size() == 0; }
     // True for non-empty arrays
    ALWAYS_INLINE constexpr
    explicit operator bool () const { return size(); }

     // Get the size of the array in elements
    ALWAYS_INLINE constexpr
    usize size () const {
        if constexpr (is_Any) {
            return impl.sizex2_with_owned >> 1;
        }
        else return impl.size;
    }

     // Maximum size differs depending on whether this array can be owned or
     // not.  The maximum size for owned arrays is the same on both 32-bit and
     // 64-bit platforms.  If you need to process arrays larger than 2 billion
     // elements, you're probably already managing your own memory anyway.
    static constexpr usize max_size_ =
        supports_owned ? uint32(-1) >> 1 : usize(-1) >> 1;
    ALWAYS_INLINE constexpr
    usize max_size () const { return max_size_; }

     // Get data pointer.  Always const by default except for UniqueArray, to
     // avoid accidental copy-on-write.
    ALWAYS_INLINE constexpr
    const T* data () const { return impl.data; }
    ALWAYS_INLINE
    T* data () requires (is_Unique) { return impl.data; }
     // Get mutable data pointer, triggering copy-on-write if needed.
    ALWAYS_INLINE
    T* mut_data () requires (supports_owned) {
        make_unique();
        return impl.data;
    }

     // Guarantees the return of a NUL-terminated buffer, possibly by attaching
     // a NUL element after the end.  Does not change the size of the array, but
     // may change its capacity.  For StaticArray and Slice, since they can't be
     // mutated, this require()s that the array is explicitly NUL-terminated.
     // (noinlining this because it shouldn't be on a hot path.)
    NOINLINE constexpr
    const T* c_str () requires (requires (T v) { !v; v = T(); }) {
        if (size() > 0 && !impl.data[size()-1]) {
        }
        else if constexpr (supports_owned) {
             // Using just reserve here instead of reserve_plenty, because
             // it's unlikely that you're going to add more onto the end
             // after requesting a NUL-terminated string.
             // Theoretically, for shared strings we could tack on a NUL byte
             // even if the shared string isn't unique, as long as that's the
             // only shared modification we do.  However, this would disallow
             // having shared strings with the same buffer and different
             // lengths, which isn't worth it.
            reserve(size() + 1);
            impl.data[size()] = T();
        }
        else require(false);
        return impl.data;
    }

     // Access individual elements.  at() and mut_at() will terminate if the
     // passed index is out of bounds.  operator[], get(), and mut_get() do not
     // do bounds checks (except on debug builds).  Only the mut_* versions can
     // trigger copy-on-write; the non-mut_* version are always const except for
     // UniqueArray.
     //
     // Note: Using &array[array.size()] to get a pointer off the end of the
     // array is NOT allowed.  Use array.end() instead, or array.data() +
     // array.size().
    ALWAYS_INLINE constexpr
    const T& at (usize i) const {
        require(i < size());
        return impl.data[i];
    }
    ALWAYS_INLINE
    T& at (usize i) requires (is_Unique) {
        return const_cast<T&>(
            const_cast<const ArrayInterface<ac, T>&>(*this).at(i)
        );
    }
    ALWAYS_INLINE
    T& mut_at (usize i) requires (supports_owned) {
        make_unique();
        return const_cast<T&>(at(i));
    }
    ALWAYS_INLINE constexpr
    const T& operator [] (usize i) const {
        expect(i < size());
        return impl.data[i];
    }
    ALWAYS_INLINE
    T& operator [] (usize i) requires (is_Unique) {
        return const_cast<T&>(
            const_cast<const ArrayInterface<ac, T>&>(*this)[i]
        );
    }
    ALWAYS_INLINE constexpr
    const T& get (usize i) const {
        return (*this)[i];
    }
    ALWAYS_INLINE
    T& get (usize i) requires (is_Unique) {
        return const_cast<T&>(
            const_cast<const ArrayInterface<ac, T>&>(*this)[i]
        );
    }
    ALWAYS_INLINE
    T& mut_get (usize i) requires (supports_owned) {
        make_unique();
        return const_cast<T&>((*this)[i]);
    }

    ALWAYS_INLINE constexpr
    const T& front () const { return (*this)[0]; }
    ALWAYS_INLINE constexpr
    T& front () requires (is_Unique) { return (*this)[0]; }
    ALWAYS_INLINE
    T& mut_front () requires (supports_owned) {
        make_unique();
        return (*this)[0];
    }

    ALWAYS_INLINE constexpr
    const T& back () const { return (*this)[size() - 1]; }
    ALWAYS_INLINE constexpr
    T& back () requires (is_Unique) { return (*this)[size() - 1]; }
    ALWAYS_INLINE
    T& mut_back () requires (supports_owned) {
        make_unique();
        return (*this)[size() - 1];
    }

     // Slice takes two offsets and does not do bounds checking (except in debug
     // builds).  Unlike operator[], the offsets are allowed to be one off the
     // end.
    ALWAYS_INLINE constexpr
    SelfSlice slice (usize start, usize end) const {
        expect(start <= size() && end <= size());
        return SelfSlice(data() + start, end - start);
    }

     // Substr takes an offset and a length, and caps both to the length of the
     // string.  Note that unlike the STL's substr, this returns a Slice/Str,
     // not a new copy.
    ALWAYS_INLINE constexpr
    SelfSlice substr (usize offset, usize length = usize(-1)) const {
        if (offset >= size()) offset = size();
        if (length > size() - offset) length = size() - offset;
        return SelfSlice(data() + offset, length);
    }

     // Get the current capacity of the owned buffer.  If this array is not
     // owned, returns 0 even if the array is not empty.
    ALWAYS_INLINE constexpr
    usize capacity () const {
        if (owned()) {
            expect(header().capacity >= min_capacity);
            return header().capacity;
        }
        else return 0;
    }

     // The minimum capacity of a shared buffer (enough elements to fill 24 (on
     // 64-bit) or 16 (on 32-bit) bytes).
    static constexpr usize min_capacity = SharableBuffer<T>::capacity_for_size(1);
    static constexpr usize max_capacity = SharableBuffer<T>::capacity_for_size(max_size_);

     // Returns if this string is owned (has a shared or unique buffer).  If
     // this returns true, then there is a SharedBufferHeader behind data().
     // Returns false for empty arrays.
    ALWAYS_INLINE constexpr
    bool owned () const {
        if constexpr (is_Any) {
            if (impl.sizex2_with_owned & 1) {
                expect(impl.data);
                return true;
            }
            else return false;
        }
        else if constexpr (supports_owned) {
            if (impl.data) return true;
            else {
                expect(!impl.size);
                return false;
            }
        }
        else return false;
    }

     // Returns if this string is unique (can be moved to a UniqueArray without
     // doing an allocation).  This is not a strict subset of owned(); in
     // particular, it will return true for most empty arrays (capacity == 0).
    ALWAYS_INLINE constexpr
    bool unique () const {
        if constexpr (is_Unique) return true;
        else if constexpr (supports_owned) {
            if (owned()) {
                return header().ref_count == 0;
            }
            else return !impl.data;
        }
        else return false;
    }

    ///// ITERATORS
    // begin(), end(), and related functions never trigger a copy-on-write, and
    // return const for all but UniqueArray.  The mut_* versions will trigger a
    // copy-on-write.

    ALWAYS_INLINE constexpr
    const T* begin () const { return impl.data; }
    ALWAYS_INLINE constexpr
    const T* cbegin () const { return impl.data; }
    ALWAYS_INLINE constexpr
    T* begin () requires (is_Unique) { return impl.data; }
    ALWAYS_INLINE
    T* mut_begin () requires (supports_owned) {
        make_unique();
        return impl.data;
    }

    ALWAYS_INLINE constexpr
    const T* end () const { return impl.data + size(); }
    ALWAYS_INLINE constexpr
    const T* cend () const { return impl.data + size(); }
    ALWAYS_INLINE constexpr
    T* end () requires (is_Unique) { return impl.data + size(); }
    ALWAYS_INLINE
    T* mut_end () requires (supports_owned) {
        make_unique();
        return impl.data + size();
    }

    ALWAYS_INLINE constexpr
    std::reverse_iterator<const T*> rbegin () const {
        return std::make_reverse_iterator(impl.data + size());
    }
    ALWAYS_INLINE constexpr
    std::reverse_iterator<const T*> crbegin () const {
        return std::make_reverse_iterator(impl.data + size());
    }
    ALWAYS_INLINE constexpr
    std::reverse_iterator<T*> rbegin () requires (is_Unique) {
        return std::make_reverse_iterator(impl.data + size());
    }
    ALWAYS_INLINE constexpr
    std::reverse_iterator<T*> mut_rbegin () requires (supports_owned) {
        make_unique();
        return std::make_reverse_iterator(impl.data + size());
    }

    ALWAYS_INLINE constexpr
    std::reverse_iterator<const T*> rend () const {
        return std::make_reverse_iterator(impl.data);
    }
    ALWAYS_INLINE constexpr
    std::reverse_iterator<const T*> crend () const {
        return std::make_reverse_iterator(impl.data);
    }
    ALWAYS_INLINE constexpr
    std::reverse_iterator<T*> rend () requires (is_Unique) {
        return std::make_reverse_iterator(impl.data);
    }
    ALWAYS_INLINE constexpr
    std::reverse_iterator<T*> mut_rend () requires (supports_owned) {
        make_unique();
        return std::make_reverse_iterator(impl.data);
    }

    ///// MUTATORS

    ALWAYS_INLINE constexpr
    void clear () { remove_ref(); impl = {}; }

     // Make sure the array is both unique and has at least enough room for the
     // given number of elements.  The final capacity might be slightly higher
     // than the requested capacity.  Never reduces capacity (use shrink_to_fit
     // to do that).  Calling with a capacity of 1 has the effect of requesting
     // the minimum owned capacity.
    ALWAYS_INLINE constexpr
    void reserve (usize cap) requires (supports_owned) {
        expect(cap <= max_size_);
        if (!unique() || cap > capacity()) {
            set_unique(reallocate(impl, cap), size());
        }
    }

     // Like reserve(), but if reallocation is needed, it doubles the capacity.
    ALWAYS_INLINE constexpr
    void reserve_plenty (usize cap) requires (supports_owned) {
        expect(cap <= max_size_);
        if (!unique() || cap > capacity())
            [[unlikely]]
        {
            set_unique(reallocate(impl, cap, capacity() * 2), size());
        }
    }

     // Make this array unique and if it has more capacity than necessary,
     // reallocate so that capacity is equal to length (rounded up to allocation
     // granularity).
    ALWAYS_INLINE constexpr
    void shrink_to_fit () requires (supports_owned) {
        if (!unique() ||
            SharableBuffer<T>::capacity_for_size(size()) < capacity()
        ) {
            set_unique(reallocate(impl, size()), size());
        }
    }

     // If this array is not uniquely owned, copy its buffer so it is uniquely
     // owned.  This is equivalent to casting to a UniqueArray/UniqueString and
     // assigning it back to self.
     // TODO: Add make_not_shared () for passing between threads
    ALWAYS_INLINE
    void make_unique () requires (supports_owned) {
        if (!unique()) {
            set_unique(reallocate(impl, size()), size());
        }
    }

     // Change the size of the array.  When growing, default-constructs new
     // elements, and will reallocate if the new size is larger than the current
     // capacity.  When shrinking, destroys elements past the new size, and
     // never reallocates.  If the current array is not unique, makes it unique
     // UNLESS it's shared and the elements are trivially destructible (which
     // means that arrays can share the same buffer despite having different
     // lengths).
    ALWAYS_INLINE
    void resize (usize new_size) requires (
        supports_owned && std::is_default_constructible_v<T>
    ) {
        usize old_size = size();
        if (new_size < old_size) shrink(new_size);
        else if (new_size > old_size) grow(new_size);
    }
    void grow (usize new_size) requires (
        supports_owned && std::is_default_constructible_v<T>
    ) {
        usize old_size = size();
        if (new_size <= old_size) [[unlikely]] return;
        reserve(new_size);
        if constexpr (std::is_trivially_default_constructible_v<T>) {
            std::memset(
                (void*)(impl.data + old_size),
                0,
                (new_size - old_size) * sizeof(T)
            );
        }
        else {
            usize i = old_size;
            try {
                for (; i < new_size; ++i) {
                    new ((void*)&impl.data[i]) T();
                }
            }
            catch (...) {
                while (i > old_size) {
                    impl.data[i].~T();
                }
                throw;
            }
        }
        set_size(new_size);
    }
    constexpr
    void shrink (usize new_size) {
        usize old_size = size();
        if (new_size >= old_size) [[unlikely]] return;
        if (std::is_trivially_destructible_v<T> || !owned()) {
             // Decrease length directly without reallocating.  This can be done
             // even on shared arrays!  But only if the element type is
             // trivially destructible, because if we don't have a canonical
             // size, we can't keep track of which elements need to be
             // destroyed.
            set_size(new_size);
        }
        else if (unique()) {
            for (usize i = old_size; i > new_size;) {
                impl.data[--i].~T();
            }
            set_size(new_size);
        }
        else if constexpr (std::is_copy_constructible_v<T>) {
            *this = UniqueArray<T>(impl.data, new_size);
        }
        else require(false);
    }
     // Nonmutating version of shrink.  Semantically equivalent to
     // slice(0, new_size), but avoids an allocate_copy for shared arrays.
    constexpr
    Self shrunk (usize new_size) const& {
        if (new_size >= size()) [[unlikely]] return *this;
        if constexpr (is_Unique) {
             // Copying then shrinking UniqueArray wastes a lot of work.
            return Self(data(), new_size);
        }
        Self r = *this;
        r.shrink(new_size);
        return r;
    }
    ALWAYS_INLINE constexpr
    Self shrunk (usize new_size) && {
        Self r = move(*this);
        r.shrink(new_size);
        return r;
    }

     // Construct an element on the end of the array, increasing its size by 1.
    template <class... Args>
    T& emplace_back (Args&&... args) requires (supports_owned) {
        reserve_plenty(size() + 1);
        T& r = *new ((void*)&impl.data[size()]) T(std::forward<Args>(args)...);
        add_size(1);
        return r;
    }

     // emplace_back but skip the capacity and uniqueness check.
    template <class... Args>
    T& emplace_back_expect_capacity (Args&&... args) requires (supports_owned) {
        expect(size() + 1 <= max_size_);
        expect(unique() && capacity() > size());
        T& r = *new ((void*)&impl.data[size()]) T(std::forward<Args>(args)...);
        add_size(1);
        return r;
    }

     // Copy-construct onto the end of the array, increasing its size by 1.
    ALWAYS_INLINE
    T& push_back (const T& v) requires (supports_owned) {
        return emplace_back(v);
    }
     // Move-construct onto the end of the array, increasing its size by 1.
    ALWAYS_INLINE
    T& push_back (T&& v) requires (supports_owned) {
        return emplace_back(move(v));
    }
    ALWAYS_INLINE
    T& push_back_expect_capacity (const T& v) requires (supports_owned) {
        return emplace_back_expect_capacity(v);
    }
    ALWAYS_INLINE
    T& push_back_expect_capacity (T&& v) requires (supports_owned) {
        return emplace_back_expect_capacity(move(v));
    }

    ALWAYS_INLINE
    void pop_back () {
        expect(size() > 0);
        shrink(size() - 1);
    }

     // Append multiple elements by copying them.
    template <ArrayIterator Ptr>
    void append (Ptr p, usize s) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        reserve_plenty(size() + s);
        copy_fill(impl.data + size(), move(p), s);
        add_size(s);
    }
    ALWAYS_INLINE
    void append (SelfSlice o) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        append(o.data(), o.size());
    }
    template <ArrayIterator Begin, ArraySentinelFor<Begin> End>
    void append (Begin b, End e) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        if constexpr (requires { usize(e - b); }) {
            return append(move(b), usize(e - b));
        }
        else if constexpr (ArrayForwardIterator<Begin>) {
            usize s = 0;
            for (auto p = b; p != e; ++p) ++s;
            return append(move(b), s);
        }
        else {
            for (auto p = move(b); p != e; ++p) {
                push_back(*b);
            }
        }
    }
     // Append uninitialized data
    ALWAYS_INLINE
    void append (Uninitialized u) requires (
        supports_owned && std::is_trivially_default_constructible_v<T>
    ) {
        reserve_plenty(size() + u.size);
        add_size(u.size);
    }

     // Append but skip the capacity check
    template <ArrayIterator Ptr>
    void append_expect_capacity (Ptr p, usize s) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        expect(size() + s <= max_size_);
        expect(unique());
        expect(capacity() >= size() + s);
        copy_fill(impl.data + size(), move(p), s);
        add_size(s);
    }
    ALWAYS_INLINE
    void append_expect_capacity (SelfSlice o) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        append_expect_capacity(o.data(), o.size());
    }
    template <ArrayIterator Begin, ArraySentinelFor<Begin> End>
    void append_expect_capacity (Begin b, End e) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        if constexpr (requires { usize(e - b); }) {
            return append_expect_capacity(move(b), usize(e - b));
        }
        else if constexpr (ArrayForwardIterator<Begin>) {
            usize s = 0;
            for (auto p = b; p != e; ++p) ++s;
            return append_expect_capacity(move(b), s);
        }
        else {
            for (auto p = move(b); p != e; ++p) {
                push_back_expect_capacity(*b);
            }
        }
    }
    ALWAYS_INLINE
    void append_expect_capacity (Uninitialized u) requires (
        supports_owned && std::is_trivially_default_constructible_v<T>
    ) {
        add_size(u.size);
    }

     // Construct an element into a specific place in the array, moving the rest
     // of the array over by 1.  If the selected constructor is noexcept,
     // constructs the element in-place, otherwise constructs it elsewhere then
     // moves it into the slot.
    template <class... Args>
    T& emplace (usize offset, Args&&... args) requires (supports_owned) {
        expect(offset < size());
        if constexpr (noexcept(T(std::forward<Args>(args)...))) {
            T* dat = do_split(impl, offset, 1);
            T* r = new ((void*)&dat[offset]) T(std::forward<Args>(args)...);
            set_unique(dat, size() + 1);
            return *r;
        }
        else {
            T v {std::forward<Args>(args)...};
            T* dat = do_split(impl, offset, 1);
            T* r;
            try {
                r = new ((void*)&dat[offset]) T(move(v));
            }
            catch (...) { never(); }
            set_unique(dat, size() + 1);
            return *r;
        }
    }
    template <class... Args> ALWAYS_INLINE
    T& emplace (const T* pos, Args&&... args) requires (supports_owned) {
        return emplace(pos - impl.data, std::forward<Args>(args)...);
    }
     // Single-element insert, equivalent to emplace.
    ALWAYS_INLINE
    T& insert (usize offset, const T& v) requires (supports_owned) {
        return emplace(offset, v);
    }
    ALWAYS_INLINE
    T& insert (const T* pos, const T& v) requires (supports_owned) {
        return emplace(pos - impl.data, v);
    }
    ALWAYS_INLINE
    T& insert (usize offset, T&& v) requires (supports_owned) {
        return emplace(offset, move(v));
    }
    ALWAYS_INLINE
    T& insert (const T* pos, T&& v) requires (supports_owned) {
        return emplace(pos - impl.data, move(v));
    }

     // Multiple-element insert.  If any of the iterator operators or the copy
     // constructor throw, the program will crash.  This is the one exception to
     // the mostly-strong exception guarantee.
    template <ArrayIterator Ptr>
    void insert (usize offset, Ptr p, usize s) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        expect(offset < size());
        if (s == 0) {
            make_unique();
        }
        else {
            T* dat = do_split(impl, offset, s);
            try {
                copy_fill(dat + offset, move(p), s);
            }
            catch (...) { require(false); }
            set_unique(dat, size() + s);
        }
    }
    template <ArrayIterator Ptr> ALWAYS_INLINE
    void insert (const T* pos, Ptr p, usize s) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        insert(pos - impl.data, move(p), s);
    }
    template <ArrayIterator Begin, ArraySentinelFor<Begin> End>
    void insert (usize offset, Begin b, End e) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        usize s;
        if constexpr (requires { usize(e - b); }) {
            s = usize(e - b);
        }
        else {
            static_assert(ArrayForwardIterator<Begin>,
                "Cannot call insert with non-sizeable single-use iterator"
            );
            s = 0; for (auto p = b; p != e; ++p) ++s;
        }
        insert(offset, move(b), s);
    }
    template <ArrayIterator Begin, ArraySentinelFor<Begin> End> ALWAYS_INLINE
    void insert (const T* pos, Begin b, End e) requires (
        supports_owned && std::is_copy_constructible_v<T>
    ) {
        insert(pos - impl.data, move(b), move(e));
    }
    ALWAYS_INLINE
    void insert (usize offset, Uninitialized u) requires (
        supports_owned && std::is_trivially_default_constructible_v<T>
    ) {
        expect(offset < size());
        if (u.size == 0) {
            make_unique();
        }
        else {
            T* dat = do_split(impl, offset, u.size);
            set_unique(dat, size() + u.size);
        }
    }
    ALWAYS_INLINE
    void insert (const T* pos, Uninitialized u) requires (
        supports_owned && std::is_trivially_default_constructible_v<T>
    ) {
        insert(pos - impl.data, u);
    }

     // Removes element(s) from the array.  If there are elements after the
     // removed ones, they will be move-assigned onto the erased elements,
     // otherwise the erased elements will be destroyed.
    ALWAYS_INLINE
    void erase (usize offset, usize count = 1) requires (supports_owned) {
        if (count == 0) {
            make_unique();
        }
        else {
            set_unique(do_erase(impl, offset, count), size() - count);
        }
    }
    ALWAYS_INLINE
    void erase (const T* pos, usize count = 1) requires (supports_owned) {
        if (count == 0) {
            make_unique();
        }
        else {
            set_unique(do_erase(impl, pos - impl.data, count), size() - count);
        }
    }
    ALWAYS_INLINE
    void erase (const T* b, const T* e) requires (supports_owned) {
        expect(e >= b);
        if (e - b == 0) {
            make_unique();
        }
        else {
            set_unique(do_erase(impl, b - impl.data, e - b), size() - (e - b));
        }
    }

    ///// INTERNAL STUFF

  private:
    ALWAYS_INLINE
    SharedBufferHeader& header () const {
        expect(supports_owned);
        return *SharableBuffer<T>::header(impl.data);
    }

    ALWAYS_INLINE constexpr
    void set_owned (T* d, usize s) {
        static_assert(supports_owned);
        expect(s <= max_size_);
        if constexpr (is_Any) {
             // If data is null, clear the owned bit so we can get away with
             // only doing one branch in owned() instead of two.
            impl.sizex2_with_owned = s << 1 | !!d;
        }
        else impl.size = s;
        impl.data = d;
    }
    ALWAYS_INLINE constexpr
    void set_unique (T* d, usize s) {
        set_owned(d, s);
        expect(unique());
    }
    ALWAYS_INLINE constexpr
    void set_unowned (const T* d, usize s) {
        static_assert(supports_static || is_Slice);
        if constexpr (is_Any) {
            impl.sizex2_with_owned = s << 1;
            impl.data = const_cast<T*>(d);
        }
        else {
            impl.size = s;
            impl.data = d;
        }
    }
    template <ArrayIterator Ptr>
    void set_copy (Ptr ptr, usize s) requires (
        std::is_copy_constructible_v<T>
    ) {
        if (s == 0) {
            impl = {}; return;
        }
        set_unique(allocate_copy(ptr, s), s);
    }
     // This noinline is a slight lie, it's actually allocate_copy that's
     // noinline
    void set_copy_noinline (const T* ptr, usize s) requires (
        std::is_copy_constructible_v<T>
    ) {
        if (s == 0) {
            impl = {}; return;
        }
        set_unique(allocate_copy_noinline(ptr, s), s);
    }
    template <ArrayIterator Begin, ArraySentinelFor<Begin> End>
    void set_copy (Begin b, End e) requires (
        std::is_copy_constructible_v<T>
    ) {
        if constexpr (requires { usize(e - b); }) {
            set_copy(move(b), usize(e - b));
        }
        else if constexpr (requires { b = b; }) {
             // If the iterator is copy-assignable that means it probably allows
             // determining its length in a separate pass.
            usize s = 0;
            for (auto p = b; p != e; ++p) ++s;
            set_copy(move(b), s);
        }
        else {
             // You gave us an iterator pair that can't be subtracted and can't
             // be copied.  Guess we'll have to keep reallocating the buffer
             // until it's big enough.
            impl = {};
            try {
                for (auto p = move(b); p != e; ++p) {
                    emplace_back(*p);
                }
            }
            catch (...) {
                this->~Self();
                impl = {};
                throw;
            }
        }
    }

    ALWAYS_INLINE constexpr
    void set_size (usize s) {
        if constexpr (is_Any) {
            impl.sizex2_with_owned = s << 1 | (impl.sizex2_with_owned & 1);
        }
        else impl.size = s;
    }
     // Just an optimization for AnyArray
    ALWAYS_INLINE constexpr
    void add_size (usize change) {
        if constexpr (is_Any) {
            impl.sizex2_with_owned += change << 1;
        }
        else impl.size += change;
    }

    ALWAYS_INLINE constexpr
    void add_ref () {
        if constexpr (supports_share) {
            if (owned()) {
                ++header().ref_count;
            }
        }
    }

    ALWAYS_INLINE constexpr
    void remove_ref () {
        if (owned()) {
            if constexpr (is_Unique) {
                expect(header().ref_count == 0);
            }
            else if constexpr (supports_owned) {
                if (header().ref_count) {
                    --header().ref_count;
                    return;
                }
            }
             // Noinline this for shared arrays with nontrivial element
             // destructors.  We don't want to inline an entire loop of
             // destructor calls in every place we remove a reference.
            if constexpr (
                supports_share && !std::is_trivially_destructible_v<T>
            ) destroy_noinline(impl);
            else destroy(impl);
        }
    }
    static
    void destroy (Impl impl) {
        Self& self = reinterpret_cast<Self&>(impl);
        for (usize i = self.size(); i > 0;) {
            impl.data[--i].~T();
        }
        SharableBuffer<T>::deallocate(impl.data);
    }
    NOINLINE static
    void destroy_noinline (Impl impl) noexcept {
        destroy(impl);
    }

    template <ArrayIterator Ptr> static
    T* copy_fill (T* dat, Ptr ptr, usize s) requires (std::is_copy_constructible_v<T>) {
        if constexpr (std::is_trivially_copy_constructible_v<T>) {
            return (T*)std::memcpy(
                (void*)dat, std::to_address(ptr), s * sizeof(T)
            );
        }
        usize i = 0;
        try {
            for (auto p = move(ptr); i < s; ++i, ++p) {
                new ((void*)&dat[i]) T(*p);
            }
        }
        catch (...) {
             // You threw from the copy constructor!  Now we have to clean up
             // the mess.
            while (i > 0) {
                dat[--i].~T();
            }
            throw;
        }
        return dat;
    }
    template <ArrayIterator Begin, ArraySentinelFor<Begin> End> static
    T* copy_fill (T* dat, Begin b, End e) requires (std::is_copy_constructible_v<T>) {
        static_assert(ArrayForwardIterator<Begin>);
        if constexpr (requires { usize(e - b); }) {
            return copy_fill(dat, move(b), usize(e - b));
        }
        else {
            usize s = 0;
            for (auto p = b; p != e; ++p) ++s;
            return copy_fill(dat, move(b), s);
        }
    }

    template <ArrayIterator Ptr> [[gnu::malloc, gnu::returns_nonnull]] static
    T* allocate_copy (Ptr ptr, usize s)
        noexcept(std::is_nothrow_copy_constructible_v<T>)
        requires (std::is_copy_constructible_v<T>)
    {
        expect(s > 0);
        T* dat = SharableBuffer<T>::allocate(s);
        try {
            return copy_fill(dat, ptr, s);
        }
        catch (...) {
            SharableBuffer<T>::deallocate(dat);
            throw;
        }
    }

    [[gnu::malloc, gnu::returns_nonnull]] NOINLINE static
    T* allocate_copy_noinline (const T* ptr, usize s) {
        return allocate_copy(ptr, s);
    }

     // Used by reserve and related functions
    [[gnu::malloc, gnu::returns_nonnull]] NOINLINE static
    T* reallocate (Impl impl, usize cap, usize cap2 = 0)
        noexcept(is_Unique || std::is_nothrow_copy_constructible_v<T>)
    {
         // Stuff non-fast-path reserve_plenty logic in here
        if (cap2 > cap) {
            cap = cap2;
            if (cap > max_size_) [[unlikely]] cap = max_size_;
        }
        Self& self = reinterpret_cast<Self&>(impl);
        usize s = self.size();
        expect(cap >= s);
        T* dat = SharableBuffer<T>::allocate(cap);
         // Can't call deallocate_owned on nullptr.
        if (!self.impl.data) return dat;
         // It's unlikely that any type will have one of copy or move be trivial
         // and not the other, so don't bother optimizing for those cases.
        if constexpr (
            std::is_trivially_copy_constructible_v<T> &&
            std::is_trivially_move_constructible_v<T> &&
            std::is_trivially_destructible_v<T>
        ) {
            dat = (T*)std::memcpy(
                (void*)dat, self.impl.data, s * sizeof(T)
            );
            self.remove_ref();
        }
        else if (self.unique()) {
            try {
                for (usize i = 0; i < s; ++i) {
                    new ((void*)&dat[i]) T(move(self.impl.data[i]));
                    self.impl.data[i].~T();
                }
            }
            catch (...) {
                 // Assume that the move constructor and destructor never throw
                 // even if they aren't marked noexcept.
                never();
            }
             // DON'T call remove_ref here because it'll double-destroy
             // self.impl.data[*]
            if (self.impl.data) {
                SharableBuffer<T>::deallocate(self.impl.data);
            }
        }
        else if constexpr (std::is_copy_constructible_v<T>) {
            try {
                copy_fill(dat, self.impl.data, s);
            }
            catch (...) {
                SharableBuffer<T>::deallocate(dat);
                 // TODO: pragma this
                throw;  // -Wno-terminate to suppress warning
            }
            --self.header().ref_count;
        }
        else require(std::is_copy_constructible_v<T>);
        return dat;
    }

     // Used by emplace and insert.  Opens a gap but doesn't put anything in it.
     // The caller must placement-new elements in the gap.
    [[gnu::returns_nonnull]] NOINLINE static
    T* do_split (Impl impl, usize split, usize shift)
        noexcept(is_Unique || std::is_nothrow_copy_constructible_v<T>)
    {
        Self& self = reinterpret_cast<Self&>(impl);
        expect(split <= self.size());
        expect(shift != 0);
        usize cap = self.capacity();
        if (self.unique() && cap >= self.size() + shift) {
             // We have enough capacity so all we need to do is move the tail.
             // Assume that the move constructor and destructor never throw even
             // if they aren't marked noexcept.
            try {
                 // Move elements forward, starting at the back
                for (usize i = self.size(); i > split; --i) {
                    new ((void*)&self.impl.data[i-1 + shift]) T(
                        move(self.impl.data[i-1])
                    );
                    self.impl.data[i-1].~T();
                }
            }
            catch (...) { never(); }
            return self.impl.data;
        }
         // Not enough capacity!  We have to reallocate, and while we're at it,
         // let's do the copy/move too.
        T* dat = SharableBuffer<T>::allocate(
            cap * 2 > self.size() + shift ?
            cap * 2 : self.size() + shift
        );
        if (cap > max_size_) [[unlikely]] cap = max_size_;
        if (self.unique()) {
             // Assume that the move constructor and destructor never throw even
             // if they aren't marked noexcept.
            try {
                for (usize i = 0; i < split; ++i) {
                    new ((void*)&dat[i]) T(move(self.impl.data[i]));
                    self.impl.data[i].~T();
                }
                for (usize i = 0; i < self.size() - split; ++i) {
                    new ((void*)&dat[split + shift + i]) T(
                        move(self.impl.data[split + i])
                    );
                    self.impl.data[split + i].~T();
                }
            }
            catch (...) { never(); }
             // Don't use remove_ref, it'll call the destructors again
            SharableBuffer<T>::deallocate(self.impl.data);
        }
        else if constexpr (std::is_copy_constructible_v<T>) { // Not unique
            usize head_i = 0;
            usize tail_i = split;
            try {
                for (; head_i < split; ++head_i) {
                    new ((void*)&dat[head_i]) T(self.impl.data[head_i]);
                }
                for (; tail_i < self.size(); ++tail_i) {
                    new ((void*)&dat[shift + tail_i]) T(
                        self.impl.data[tail_i]
                    );
                }
            }
            catch (...) {
                 // Yuck, someone threw an exception in a copy constructor!
                while (tail_i > split) {
                    dat[shift + --tail_i].~T();
                }
                while (head_i > 0) {
                    dat[--head_i].~T();
                }
                throw;
            }
            --self.header().ref_count;
        }
        else never();
        return dat;
    }

    [[gnu::returns_nonnull]] NOINLINE static
    T* do_erase (Impl impl, usize offset, usize count)
        noexcept(is_Unique || std::is_nothrow_copy_constructible_v<T>)
    {
        Self& self = reinterpret_cast<Self&>(impl);
        usize old_size = self.size();
        expect(count != 0);
        expect(offset <= old_size && offset + count <= old_size);
        if (self.unique()) {
            try {
                 // Move some elements over.  The destination will always
                 // still exist so use operator=
                for (usize i = offset; count + i < old_size; ++i) {
                    self.impl.data[i] = move(
                        self.impl.data[count + i]
                    );
                }
                 // Then delete the rest
                for (usize i = old_size - count; i < old_size; ++i) {
                    self.impl.data[i].~T();
                }
            }
            catch (...) { never(); }
            return self.impl.data;
        }
        else if constexpr (std::is_copy_constructible_v<T>) {
             // Not unique, so copy instead of moving
            T* dat = SharableBuffer<T>::allocate(old_size - count);
            usize i = 0;
            try {
                for (; i < offset; i++) {
                    new ((void*)&dat[i]) T(self.impl.data[i]);
                }
                for (; i < old_size - count; i++) {
                    new ((void*)&dat[i]) T(self.impl.data[count + i]);
                }
            }
            catch (...) {
                 // If an exception happens we have to destroy the
                 // incompletely constructed target array.  Fortunately,
                 // unlike in do_split, the target array is completely
                 // contiguous.
                while (i > 0) {
                    dat[--i].~T();
                }
                SharableBuffer<T>::deallocate(dat);
                throw;
            }
            --self.header().ref_count;
            return dat;
        }
        else never();
    }
};

///// OPERATORS

 // Make this template as generic as possible but nail one side down to
 // ArrayInterface.
template <ArrayClass ac, class T, class B>
constexpr bool operator== (
    const ArrayInterface<ac, T>& a, const B& b
) requires (
    requires { usize(b.size()); a.data()[0] == b.data()[0]; }
) {
    usize as = a.size();
    usize bs = b.size();
    const T* ad = a.data();
    const auto& bd = b.data();
    if (as != bs) return false;
     // Unlike most STL containers, this WILL short-circuit if the arrays have
     // the same data pointer and size.
    if constexpr (requires { ad == bd; }) {
        if (ad == bd) return true;
    }
    if constexpr (std::is_scalar_v<T>) {
        return std::memcmp(ad, bd, as) == 0;
    }
    else {
        for (usize i = 0; i < as; ++i) {
            if (!(ad[i] == bd[i])) {
                return false;
            }
        }
        return true;
    }
}
 // Allow comparing to raw char arrays for string types only.
template <ArrayClass ac, class T, usize len>
ALWAYS_INLINE constexpr
bool operator== (
    const ArrayInterface<ac, T>& a, const T(& b )[len]
) requires (ArrayInterface<ac, T>::is_String) {
    expect(!b[len-1]);
    usize as = a.size();
    usize bs = len - 1;
    const T* ad = a.data();
    const auto& bd = b;
    if (as != bs) return false;
    if constexpr (std::is_scalar_v<T>) {
        return std::memcmp(ad, bd, bs) == 0;
    }
    else {
        for (usize i = 0; i < bs; ++i) {
            if (!(ad[i] == bd[i])) {
                return false;
            }
        }
        return true;
    }
}

 // I can't be bothered to learn what <=> is supposed to return.  They should
 // have just made it int.
template <ArrayClass ac, class T, class B>
constexpr auto operator<=> (
    const ArrayInterface<ac, T>& a, const B& b
) requires (
    requires { usize(b.size()); a.data()[0] <=> b.data()[0]; }
) {
    usize as = a.size();
    usize bs = b.size();
    const T* ad = a.data();
    const auto& bd = b.data();
    if constexpr (requires { ad == bd; }) {
        if (as == bs && ad == bd) return 0 <=> 0;
    }
    for (usize i = 0; i < as && i < bs; ++i) {
        auto res = ad[i] <=> bd[i];
        if (res != (0 <=> 0)) return res;
    }
    return as <=> bs;
}
template <ArrayClass ac, class T, usize len>
ALWAYS_INLINE constexpr
auto operator<=> (
    const ArrayInterface<ac, T>& a, const T(& b )[len]
) requires (ArrayInterface<ac, T>::is_String) {
    expect(!b[len-1]);
    usize as = a.size();
    usize bs = len - 1;
    const T* ad = a.data();
    const auto& bd = b;
    for (usize i = 0; i < as && i < bs; ++i) {
        auto res = ad[i] <=> bd[i];
        if (res != (0 <=> 0)) return res;
    }
    return as <=> bs;
}

} // arrays
} // uni

namespace std {
template <uni::ArrayClass ac, class T>
struct hash<uni::ArrayInterface<ac, T>> {
    uni::usize operator() (const uni::ArrayInterface<ac, T>& a) const {
         // Just do an x33 hash (djb2) on whatever std::hash returns for the
         // contents.  At least for libstdc++, hash is a no-op on basic integer
         // types, so for char strings this just does an x33 hash on the string.
         //
         // This is fast but vulnerable to hash denial attacks.
        uni::usize r = 5381;
        for (auto& e : a) {
            r = (r << 5) + r + std::hash<T>{}(e);
        }
        return r;
    }
};
}
