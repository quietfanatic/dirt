#include "lilac.h"

#include <new>

void* operator new (uni::usize s) noexcept {
    return uni::lilac::allocate(s);
}
void* operator new[] (uni::usize s) noexcept {
    return uni::lilac::allocate(s);
}
void* operator new (uni::usize s, const std::nothrow_t&) noexcept {
    return uni::lilac::allocate(s);
}
void* operator new[] (uni::usize s, const std::nothrow_t&) noexcept {
    return uni::lilac::allocate(s);
}
void operator delete (void* p) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
void operator delete[] (void* p) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
void operator delete (void* p, const std::nothrow_t&) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
void operator delete[] (void* p, const std::nothrow_t&) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
 // Compiler warns if you override unsized delete without overriding sized
 // delete as well.  However, depending on the design of both the application
 // and the libraries it uses, the passed size may be incorrect, so it should be
 // ignored.
void operator delete (void* p, uni::usize) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
void operator delete[] (void* p, uni::usize) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
void operator delete (void* p, uni::usize, const std::nothrow_t&) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
void operator delete[] (void* p, uni::usize, const std::nothrow_t&) noexcept {
    uni::lilac::deallocate_unknown_size(p);
}
