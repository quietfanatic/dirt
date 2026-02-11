 // The "Universe" manages the set of loaded resources and related global data.

#pragma once
#include <memory>
#include "../../uni/indestructible.h"
#include "../common.h"
#include "resource-scheme.h"
#include "resource.h"

namespace ayu::in {

struct ResourceData : Resource {
    ResourceState state = RS::Unloaded;
     // These are only used during reachability scanning, but we have extra room
     // for them here.
    bool root;
    bool reachable;
     // This is also only used during reachability scanning, but storing it
     // externally would require using an unordered_map (to use a UniqueArray,
     // we need an integer index, but that's what this itself is).
    u32 node_id;
    IRI name;
    AnyVal value {};
    ResourceData (const IRI& n) : name(n) { }
};

struct Universe {
    UniqueArray<Hashed<ResourceRef>> resources;
    UniqueArray<Hashed<const ResourceScheme*>> schemes;
    UniqueArray<AnyPtr> tracked;

    ResourceRef get_resource (const IRI&);
    void delete_resource (ResourceRef);
    void register_scheme (const ResourceScheme*);
    void unregister_scheme (const ResourceScheme*);
    const ResourceScheme* require_scheme (const IRI&);
};

 // The memory leak detector flags the universe's resources as leaked,
 // because at program close, the array of resource refs is destroyed
 // without destroying the resources.  How do we solve that?  By leaking the
 // array too!
constinit inline Indestructible<Universe> g_universe;

} // namespace ayu::in

