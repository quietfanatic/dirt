#pragma once
#include "resource.h"

#include "../../uni/indestructible.h"
#include "../common.h"
#include "scheme.h"

namespace ayu::in {

 // The "Universe" manages the set of loaded resources and related global data.
 // There isn't a whole lot of meaning to keeping this data all together.

struct Universe {
     // Embed hashes in the array so we don't have to dereference each pointer
     // when we search the array.
    UniqueArray<Hashed<ResourceRef>> resources;
    UniqueArray<Hashed<ResourceScheme*>> schemes;
    UniqueArray<AnyPtr> tracked;
};

 // The memory leak detector flags the universe's resources as leaked,
 // because at program close, the array of resource refs is destroyed
 // without destroying the resources.  How do we solve that?  By leaking the
 // array too!
constinit inline Indestructible<Universe> g_universe;

} // namespace ayu::in

