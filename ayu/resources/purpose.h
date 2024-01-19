#pragma once
#include "resource.h"

namespace ayu {

struct Purpose {
    UniqueArray<Resource> resources;
     // Make sure these resources are loaded and add them to the purpose.
    void acquire (Slice<Resource>);
    void acquire (Resource r) { acquire(Slice<Resource>{r}); }
     // Remove these resources from the purpose.  If this is the last purpose
     // with these resources, they will be unloaded.  If passed a resource that
     // is not in this purpose, throws e_ResourceNotInPurpose.
    void release (Slice<Resource>);
    void release (Resource r) { release(Slice<Resource>{r}); }
     // Remove all resources from this purpose.
    void release_all ();
    ~Purpose () { release_all(); }
};

constexpr ErrorCode e_ResourceNotInPurpose = "ayu::e_ResourceNotInPurpose";

} // ayu
