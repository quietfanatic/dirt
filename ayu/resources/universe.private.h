 // The "Universe" manages the set of loaded resources and related global data.

#pragma once
#include <memory>
#include "../../uni/indestructible.h"
#include "../common.h"
#include "resource-scheme.h"
#include "resource.h"

namespace ayu::in {

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

