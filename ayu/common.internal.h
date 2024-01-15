#pragma once

#include "common.h"

namespace ayu::in {

 // Predeclare some private classes
struct DocumentData;
struct Description;
struct LocationData;
void delete_LocationData (LocationData*) noexcept;
struct ResourceData;

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
            reinterpret_cast<RefCounted*>(p)->ref_count++;
        }
    }
    ALWAYS_INLINE constexpr void dec () {
        if (p && !--reinterpret_cast<RefCounted*>(p)->ref_count) {
            deleter(p);
        }
    }

    ALWAYS_INLINE constexpr RCP (Null n = null) : p(n) { }
    ALWAYS_INLINE constexpr RCP (T* p) : p(p) { inc(); }
    ALWAYS_INLINE constexpr RCP (const RCP& o) : p(o.p) { inc(); }
    ALWAYS_INLINE RCP (RCP&& o) : p(o.p) { o.p = null; }
    ALWAYS_INLINE constexpr ~RCP () { dec(); }

    ALWAYS_INLINE RCP& operator = (const RCP& o) { dec(); p = o.p; inc(); return *this; }
    ALWAYS_INLINE RCP& operator = (RCP&& o) { dec(); p = o.p; o.p = null; return *this; }

     // It's up to the owning class to maintain const-correctness.
    ALWAYS_INLINE T& operator * () const { return *p; }
    ALWAYS_INLINE T* operator -> () const { return p; }
    ALWAYS_INLINE explicit operator bool () const { return p; }
};
template <class T, void(& deleter )(T*)> ALWAYS_INLINE
bool operator == (const RCP<T, deleter>& a, const RCP<T, deleter>& b) {
    return a.p == b.p;
}
template <class T, void(& deleter )(T*)> ALWAYS_INLINE
bool operator != (const RCP<T, deleter>& a, const RCP<T, deleter>& b) {
    return a.p != b.p;
}

}  // namespace ayu::in
