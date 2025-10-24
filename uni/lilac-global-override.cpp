#include "lilac.h"

#include <new>

ALWAYS_INLINE void* operator new (uni::usize s) {
    return uni::lilac::allocate(s);
}
ALWAYS_INLINE void* operator new[] (uni::usize s) {
    return uni::lilac::allocate(s);
}
ALWAYS_INLINE void* operator new (uni::usize s, const std::nothrow_t&) noexcept {
    return uni::lilac::allocate(s);
}
ALWAYS_INLINE void* operator new[] (uni::usize s, const std::nothrow_t&) noexcept {
    return uni::lilac::allocate(s);
}
ALWAYS_INLINE void operator delete (void* p) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
ALWAYS_INLINE void operator delete[] (void* p) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
ALWAYS_INLINE void operator delete (void* p, const std::nothrow_t&) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
ALWAYS_INLINE void operator delete[] (void* p, const std::nothrow_t&) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
 // Compiler warns if you override unsized delete without overriding sized
 // delete as well.  However, because the standard library ignores the size,
 // some applications and libraries pass an incorrect size, so sadly it must be
 // ignored.  (This most commonly happens when deleting a derived object through
 // a pointer to base).
ALWAYS_INLINE void operator delete (void* p, uni::usize) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
ALWAYS_INLINE void operator delete[] (void* p, uni::usize) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
ALWAYS_INLINE void operator delete (void* p, uni::usize, const std::nothrow_t&) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
ALWAYS_INLINE void operator delete[] (void* p, uni::usize, const std::nothrow_t&) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
