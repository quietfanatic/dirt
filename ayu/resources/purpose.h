#pragma once
#include "resource.h"

namespace ayu {

struct Purpose {
    UniqueArray<SharedResource> resources;
     // Make sure this resources is loaded and add it to the purpose.  If
     // loading the resources causes more resources to be loaded, add them to
     // this purpose as well.  Can be rolled back with a ResourceTransaction.
    void acquire (ResourceRef);
     // Remove these resources from the purpose.  If this is the last purpose
     // with these resources, they will be unloaded.  If passed a resource that
     // is not in this purpose, throws e_ResourceNotInPurpose.  Note that this
     // will not release resources that were loaded as a result of these
     // resources being loaded.  Can be rolled back with a ResourceTransaction.
    void release (ResourceRef);
     // Remove all resources from this purpose.  Can be rolled back with a
     // ResourceTransaction.
    void release_all ();
    ~Purpose () { release_all(); }
};

 // The purpose resources will be acquired by if there is not other current
 // purpose.
extern Purpose general_purpose;
 // Whatever purpose is currently acquiring or releasing resources.
extern Purpose* current_purpose;

 // Thrown when trying to release a resource from a purpose that hasn't acquired
 // that purpose.
constexpr ErrorCode e_ResourceNotInPurpose = "ayu::e_ResourceNotInPurpose";

} // ayu
