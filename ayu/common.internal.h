#pragma once

#include "common.h"

//#define AYU_PROFILE
#ifdef AYU_PROFILE
#include <cstdio>
#include <ctime> // Will be using POSIX functions though
#endif

namespace ayu::in {

#ifdef AYU_PROFILE
inline void plog (const char* s) {
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    fprintf(stderr, "[%ld.%09ld] %s\n", long(t.tv_sec), long(t.tv_nsec), s);
}
#else
inline void plog (const char*) { }
#endif

 // Predeclare some private classes
struct DocumentData;
struct Description;
void delete_Location (const Location*) noexcept;
void delete_Resource_if_unloaded (Resource*) noexcept;

 // Intrusive reference counting
struct RefCounted {
    mutable uint32 ref_count = 0;
};
 // T must be BINARY-COMPATIBLE with RefCounted.
 // This means RefCounted must be the FIRST BASE and NO VIRTUAL METHODS.
 // I haven't thought of a way to enforce this yet.
 // The benefit to this is that T need not be complete to use this class.
template <class T, void(& deleter )(T*)>
struct RCP {
    T* p;

    ALWAYS_INLINE constexpr void inc () {
        if (p) {
            reinterpret_cast<const RefCounted*>(p)->ref_count++;
        }
    }
    ALWAYS_INLINE constexpr void dec () {
        if (p && !--reinterpret_cast<const RefCounted*>(p)->ref_count) {
            deleter(p);
        }
    }

    ALWAYS_INLINE constexpr RCP (Null n = null) : p(n) { }
    ALWAYS_INLINE constexpr RCP (T* p) : p(p) { inc(); }
    ALWAYS_INLINE constexpr RCP (const RCP& o) : p(o.p) { inc(); }
    ALWAYS_INLINE constexpr RCP (RCP&& o) : p(o.p) { o.p = null; }
    ALWAYS_INLINE constexpr ~RCP () { dec(); }

    ALWAYS_INLINE constexpr RCP& operator = (const RCP& o) { dec(); p = o.p; inc(); return *this; }
    ALWAYS_INLINE constexpr RCP& operator = (RCP&& o) { dec(); p = o.p; o.p = null; return *this; }

     // It's up to the owning class to maintain const-correctness.
    ALWAYS_INLINE constexpr T& operator * () const { return *p; }
    ALWAYS_INLINE constexpr T* operator -> () const { return p; }
    ALWAYS_INLINE constexpr explicit operator bool () const { return p; }
};
template <class T, void(& deleter )(T*)> ALWAYS_INLINE
bool operator == (const RCP<T, deleter>& a, const RCP<T, deleter>& b) {
    return a.p == b.p;
}
template <class T, void(& deleter )(T*)> ALWAYS_INLINE
bool operator != (const RCP<T, deleter>& a, const RCP<T, deleter>& b) {
    return a.p != b.p;
}

template <class T>
struct Hashed {
    usize hash;
    T value;
};

}  // namespace ayu::in
