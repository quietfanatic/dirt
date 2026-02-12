#pragma once
#include "resource.h"

#include "../../uni/indestructible.h"
#include "../common.h"
#include "resource-scheme.h"

namespace ayu::in {

 // The "Universe" manages the set of loaded resources and related global data.

struct Universe {
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

