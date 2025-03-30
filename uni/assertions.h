#pragma once
#include <exception>
#include <source_location>
#include "common.h"

namespace uni {
inline namespace assertions {

 // Abort if the condition isn't true.
template <class T> ALWAYS_INLINE static constexpr
T&& require (
    T&& v, std::source_location loc = std::source_location::current()
);

 // Either aborts or triggers undefined behavior if the condition isn't true,
 // depending on NDEBUG.  Always evaluates the argument in either case.  If the
 // argument can't be optimized out, check NDEBUG yourself.
 //
 // Note that occasionally using this makes optimization worse instead of
 // better, which seems to be more likely when you use the return value.
#ifndef NDEBUG
template <class T> ALWAYS_INLINE static constexpr
T&& expect (
    T&& v, std::source_location loc = std::source_location::current()
);
#else
template <class T> ALWAYS_INLINE static constexpr
T&& expect (T&& v);
#endif

[[noreturn, gnu::cold]]
void abort_requirement_failed (
    std::source_location = std::source_location::current()
) noexcept;

 // Equivalent to expect(false) but doesn't warn about lack of return
#ifdef NDEBUG
[[noreturn]] ALWAYS_INLINE static
void never () {
#if HAS_BUILTIN(__builtin_unreachable)
    __builtin_unreachable();
#elif _MSC_VER
    __assume(false);
#else
    *(int*)null = 0;
#endif
}
#else
[[noreturn]] ALWAYS_INLINE static
void never (std::source_location loc = std::source_location::current()) {
    abort_requirement_failed(loc);
}
#endif

template <class T> ALWAYS_INLINE static constexpr
T&& require (T&& v, std::source_location loc) {
    if (!v) [[unlikely]] abort_requirement_failed(loc);
    return std::forward<T>(v);
}

#ifndef NDEBUG
template <class T> ALWAYS_INLINE static constexpr
T&& expect (T&& v, std::source_location loc) {
    if (!v) [[unlikely]] never(loc);
    return std::forward<T>(v);
}
#else
template <class T> ALWAYS_INLINE static constexpr
T&& expect (T&& v) {
    if (!v) [[unlikely]] never();
    return std::forward<T>(v);
}
#endif

} // assertions
} // uni
