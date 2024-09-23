#include "lilac.h"

void* operator new (uni::usize s) {
    return uni::lilac::allocate(s);
}
void operator delete (void* p) {
    uni::lilac::deallocate_unknown_size(p);
}
 // Keep the compiler from warning that this isn't defined; however, the
 // passed-in size may not be correct if the application deletes a derived class
 // via a pointer to base class.
void operator delete (void* p, uni::usize) {
    uni::lilac::deallocate_unknown_size(p);
}
