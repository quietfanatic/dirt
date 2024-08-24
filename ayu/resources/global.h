#pragma once
#include "../common.h"

namespace ayu {

 // Registers a global variable that is known to ayu.  This allows the resource
 // system to keep the global updated when reload() is called, and if the global
 // refers to something inside a resource, that resource will not be unloaded.
 //
 // Things in resources cannot reference globals; if they do, they will become
 // unserializable, because globals do not have an associated Location.
 //
 // If you're registering a global pointer, make sure to pass a pointer to the
 // pointer, not the pointer itself!
void global (const AnyPtr&);

void unregister_global (const AnyPtr&);

} // ayu
