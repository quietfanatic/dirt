#pragma once
#include <cassert>
#include <source_location>
#include "common.h"

namespace uni {

 // Abort execution after printing an error message
[[noreturn, gnu::cold]]
void abort_assertion_failed (
    std::source_location = std::source_location::current()
) noexcept;

 // Aborts if the condition isn't true in all build configurations.
template <class T> ALWAYS_INLINE static constexpr
T&& require (
    T&& v, std::source_location loc = std::source_location::current()
) {
    if (!v) abort_assertion_failed(loc);
    return std::forward<T>(v);
}

 // On debug builds this is like require().  On release builds, this triggers
 // undefined behavior if its argument is falsey, which is useful for
 // optimization.  Unlike [[assume]], this always evaluates its argument.
 //
 // Note that occasionally this makes optimization worse instead of better.
 // This is more likely if there are many expect()s in a row, or if the return
 // value is used, or if there's a branch in the argument that's similar to
 // another branch outside (which can affect that branch even if the argument is
 // optimizaed away).
 //
 // If you want to have a debug assert that completely disappears in release
 // builds, then just use assert().
 //
 // If you want the same behavior except that the argument is not evaluated in
 // release builds, use assert() then [[assume]].
#ifndef NDEBUG
template <class T> ALWAYS_INLINE static constexpr
T&& expect (
    T&& v, std::source_location loc = std::source_location::current()
) {
    if (!v) abort_assertion_failed(loc);
    return std::forward<T>(v);
}
#else
template <class T> ALWAYS_INLINE static constexpr
T&& expect (T&& v) {
    assert(!!v);
    [[assume(!!v)]];
    return std::forward<T>(v);
}
#endif

 // Equivalent to expect(false) but doesn't warn about lack of return
#ifndef NDEBUG
[[noreturn]] ALWAYS_INLINE static
void never (std::source_location loc = std::source_location::current()) {
    abort_assertion_failed(loc);
}
#else
[[noreturn]] ALWAYS_INLINE static
void never () { [[assume(false)]]; }
#endif

} // uni
