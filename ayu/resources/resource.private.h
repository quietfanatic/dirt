#pragma once
#include "resource.h"

#include "../../uni/indestructible.h"
#include "../common.h"
#include "extension.h"
#include "scheme.h"

namespace ayu::in {

 // The "Universe" manages the set of loaded resources and related global data.
 // There isn't a whole lot of meaning to keeping this data all together,
 // besides to reduce the number of global variable loads, which are kinda fat
 // in modern C++.

struct ResourceSchemeEntry {
    usize hash;
    SharedString name;
    ResourceScheme* scheme;
};

struct ResourceExtensionEntry {
    usize hash;
    SharedString name;
    ResourceExtension* extension;
};

struct Universe {
     // Embed hash in the array so we don't have to dereference each pointer
     // when we search for a Resource.
    UniqueArray<Hashed<ResourceRef>> resources;
     // Embed name too in schemes and extensions, so we can register the same
     // one with multiple names.
    UniqueArray<ResourceSchemeEntry> schemes;
    UniqueArray<ResourceExtensionEntry> extensions;
    ResourceExtension* default_extension = null;
    UniqueArray<AnyPtr> tracked;
};

 // The memory leak detector flags the universe's resources as leaked,
 // because at program close, the array of resource refs is destroyed
 // without destroying the resources.  How do we solve that?  By leaking the
 // array too!
constinit inline Indestructible<Universe> g_universe;

} // namespace ayu::in

