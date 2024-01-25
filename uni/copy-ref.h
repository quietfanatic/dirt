// CopyRef<> / CRef<>
// Pass-by-const-reference semantics with pass-by-value performance.
//
// CopyRef<T> is a class that acts like a const reference to an object of type
// T, but it's representation is actually a bit copy of the object.
//
// Alternatively, CopyRef<T> can be though of as a way to copy objects while
// dodging their copy constructors and destructors, e.g. for an object that
// counts references this skips the reference increment/decrement.
//
// CopyRef<T> is only recommended for objects small enough to be passed by value
// in the current ABI.  For most ABIs this is the size of two pointers (it's one
// pointer on Microsoft x64).
//
// You can use CopyRef<T> if all of the following are true, but most of these
// requirements are not enforcible in current C++.
//   - T is movable and not trivially copiable.
//   - T has no members marked as mutable.
//   - T has no behavior that depends on its address staying constant.
//   - T will not be modified by other code while you have a reference to it.
//
// ConstRef<T> is a thin wrapper around const T&, to be used with CRef<T>.
//
// CRef<T> selects between CopyRef<T> or ConstRef<T> depending on the size of T
// and the current ABI.  If T has not been defined yet, its size cannot be
// determined, so you have to pass the size as a second template parameter e.g.
// CRef<T, 16>.
//
// Like all reference-like types, undefined behavior will result if you keep a
// CopyRef<T> or CRef<T> around longer than the lifetime of the object it
// references.  Unlike const T&, undefined behavior also results if other code
// modifies the original while you have a reference.
//
// Note: The reason CopyRef<T> requires move constructibility even though it
// doesn't do any moves is because if a type is not move-constructible, that
// usually implies that its address is semantically important for it, and the
// CopyRef would have a different address from the original.  The requirement of
// non-trivial copyability isn't really a requirement, but if the object is
// trivially copyable then there's no reason to use CopyRef, so just pass by
// value.

#pragma once

#include <cstring>
#include <utility>
#include "common.h"
#include "assertions.h"

namespace uni {

template <class T>
struct CopyRef {
     // Default constructible, but if you reference this before assigning to it,
     // the behavior will be very undefined.
    CopyRef () = default;
     // Make trivially copyable
    CopyRef (const CopyRef<T>&) = default;
     // Implicit coercion from const T&
     // Sadly there is no way to make this constexpr (std::bit_cast only works
     // for types that are already trivially copyable).
    ALWAYS_INLINE CopyRef (const T& t) :
        CopyRef(reinterpret_cast<const CopyRef<T>&>(t))
    { }
     // Implicit coercion to const T&
    ALWAYS_INLINE operator const T& () const {
        return *reinterpret_cast<const T*>(repr);
    }
     // Wasn't sure whether to overload this or not, but if the class implements
     // this, you probably want to use whatever *T returns instead of
     // CopyRef<T>.
    ALWAYS_INLINE const T& operator* () const {
        return *reinterpret_cast<const T*>(repr);
    }
     // Sadly we can't overload ., so here's the next best alternative
    ALWAYS_INLINE const T* operator-> () const {
        return reinterpret_cast<const T*>(repr);
    }
     // Allow assigning
    ALWAYS_INLINE CopyRef<T>& operator= (const CopyRef<T>& o) {
         // Can't default this because you can't assign arrays.
        std::memcpy(repr, o.repr, sizeof(T));
        return *this;
    }
     // Forbid assigning from a T, as that's probably a mistake (T's copy
     // constructor and copy assigner will not be called).  Explictly cast to
     // CopyRef<T> first.
    CopyRef<T>& operator= (const T&) = delete;
     // Because C++ doesn't have Perl's ref->[i] and ref->(foo) syntax, and
     // nobody wants to write (*ref)[i]
    template <class Ix>
    ALWAYS_INLINE auto operator [] (Ix&& i) const {
        return (**this)[std::forward<Ix>(i)];
    }
    template <class... Args>
    ALWAYS_INLINE auto operator () (Args&&... args) const {
        return (**this)(std::forward<Args>(args)...);
    }
  private:
    alignas(T) char repr [sizeof(T)];
};

 // For use with CRef to be source-compatible with CopyRef.  If you're going to
 // use this directly, just use const T& instead.
template <class T>
struct ConstRef {
    ALWAYS_INLINE ConstRef (const ConstRef<T>&) = default;
    ALWAYS_INLINE ConstRef (const T& ref) : ref(ref) { }
    ALWAYS_INLINE operator const T& () const { return ref; }
    ALWAYS_INLINE const T& operator* () const { return ref; }
    ALWAYS_INLINE const T* operator-> () const { return &ref; }
    ConstRef<T>& operator= (const ConstRef<T>&) = delete;
  private:
    const T& ref;
};

 // Most ABIs support pass-by-value of up to twice the size of a register.  The
 // most major exception is Microsoft x64.
#ifndef CONFIG_PASS_BY_VALUE_MAX_SIZE
#if _MSC_VER && _M_AMD64
#define CONFIG_PASS_BY_VALUE_MAX_SIZE 8
#else
#define CONFIG_PASS_BY_VALUE_MAX_SIZE (2*sizeof(void*))
#endif
#endif

template <class T, usize size = sizeof(T)>
using CRef = std::conditional_t<
    size <= CONFIG_PASS_BY_VALUE_MAX_SIZE,
    CopyRef<T>, ConstRef<T>
>;

 // MoveRef<T> is a wrapper class that behaves like T&&, but is more optimizable
 // (and slightly more dangerous).
 //
 // Whenever a MoveRef<T> is created, it MUST be moved from EXACTLY ONCE with
 // the syntax *move(ref).  Be aware of:
 //   - Exception unwinding; if a function throws an exception while a MoveRef
 //     is active, the MoveRef will be dropped.
 //   - Argument evaluation order; the compiler is free to evaluate function
 //     arguments in any order.  If you have an expression like
 //         shortcuts.emplace_back(*move(name), parse_term())
 //     the compiler might run parse_term() before *move(name), and if
 //     parse_term() throws an exception, the MoveRef will be dropped.
 //
 // To be safe, it's a good idea to immediately move out of a MoveRef at the
 // beginning of a function.
 //     void set_shortcut(MoveRef<AnyString> name_) {
 //         auto name = *move(name_);
 //         ...
 //     }
 //
 // Technical differences between MoveRef<T> and T&&:
 //   - Using a MoveRef<T> after it has been moved from is an error (assert in
 //     debug builds, undefined behavior in release builds).
 //     (detected at runtime in debug builds).
 //   - Failing to move from a MoveRef<T> before it goes out of scope is
 //     an error (assert in debug builds, undefined behavior in release builds).
 //   - When a function takes a MoveRef<T>, it guarantees that it will take
 //     ownership of the T, so the caller can optimize away the destructor.
 // In contrast:
 //   - Moving from a T&& is required to leave it in a defined state, so after
 //     moving from it, it can still be used and must still be destroyed.
 //   - When a function takes a T&&, there's no guarantee it will actually move
 //     from the T, so the caller can't optimize away the destructor, even if
 //     the programmer knows it could.
 // Passing by value is actually very similar to passing by T&&; the caller is
 // responsible for constructing and destructing the T, and the callee may or
 // may not move from it.
 //
 // When to use MoveRef<T> vs. T&&:
 //   - If the function will be inlined, the compiler can optimize passing T&&
 //     and T (value) pretty well, so there isn't much need to use MoveRef<T>.
 //   - If the function will not be inlined (it's NOINLINE or crosses a module
 //     boundary) then the compiler will probably optimize MoveRef<T> better.
 //   - MoveRef<T> uses pass-by-value ABI, so if your object is larger than
 //     about two pointers, It may be better to use T&& instead.
 //   - If T's destructor can't be optimized away (like if it's NOINLINE) then
 //     MoveRef<T> won't help as much.
 //
 // MoveRef has a different sizeof and type traits between debug and release
 // builds, so don't store it anywhere and don't pass it to generic templates.

template <class T>
struct MoveRef {
#ifndef NDEBUG
     // In debug builds, forbid copying and moving
    MoveRef () = delete;
    MoveRef (const MoveRef&) = delete;
#else
     // In release builds, make the type trivially copyable so the ABI passes it
     // in registers.
    MoveRef () = default;
    MoveRef (const MoveRef&) = default;
#endif
    MoveRef (MoveRef&&) = delete;
    MoveRef& operator= (const MoveRef&) = delete;
     // Construction
    template <class... Args>
    ALWAYS_INLINE MoveRef (Args&&... args) requires (
        requires { T(std::forward<Args>(args)...); }
    ) {
#ifndef NDEBUG
        active = true;
#endif
        new (repr) T(std::forward<Args>(args)...);
         // Don't destroy t, the caller will destroy it.
    }
     // Temporarily access.  TODO: remove these, they seem dangerous.
    ALWAYS_INLINE T& operator* () & {
#ifndef NDEBUG
        expect(active);
#endif
        return *reinterpret_cast<T*>(repr);
    }
    ALWAYS_INLINE T* operator-> () & {
#ifndef NDEBUG
        expect(active);
#endif
        return reinterpret_cast<T*>(repr);
    }
     // Move back to a T value.  The object is no longer leakable.  Coercion
     // with "operator T () &&" doesn't seem to work.
    ALWAYS_INLINE T operator* () && {
#ifndef NDEBUG
        expect(active);
#endif
        T r = move(reinterpret_cast<T&>(repr));
        reinterpret_cast<T&>(repr).~T();
#ifndef NDEBUG
        active = false;
#endif
        return r;
    }
    T* operator-> () && {
         // We could make this work, but it would be a bit of work, and it'd be
         // easy to make the destructor run at the wrong time.
        static_assert((T*)null,
            "Cannot use operator-> on rvalue MoveRef.  "
            "Use (*move(ref)).member instead"
        );
    }

#ifndef NDEBUG
    ~MoveRef () {
        expect(!active);
    }
#endif

  private:
    alignas(T) char repr [sizeof(T)];
#ifndef NDEBUG
    bool active;
#endif
};

} // uni
