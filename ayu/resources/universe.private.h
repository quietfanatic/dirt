 // The "Universe" manages the set of loaded resources and related global data.

#pragma once
#include <memory>
#include "../../uni/indestructible.h"
#include "../common.h"
#include "resource.h"
#include "scheme.h"

namespace ayu::in {
using namespace iri::literals;

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

 // The default scheme.  Don't auto_activate because this is only used when
 // there are no active schemes.
inline auto default_scheme = FolderResourceScheme("file", "file:/"_iri, false);

struct Universe {
    UniqueArray<Hashed<ResourceRef>> resources;
    UniqueArray<Hashed<const ResourceScheme*>> schemes;
    UniqueArray<AnyPtr> tracked;

    ResourceRef get_resource (const IRI& name) {
        Str spec = expect(name.spec());
        expect(spec.begin() < spec.end());
        usize h = uni::hash(spec);

        for (auto& r : resources) {
            if (r.hash == h && r.value->name().spec_ == spec) {
                return r.value;
            }
        }
        auto data = new ResourceData(name);
        return resources.emplace_back(h, data).value;
    }

    void delete_resource (ResourceRef r) {
        for (auto& e : resources) {
            if (e.value == r) {
                resources.erase(&e);
                delete r.data;
                return;
            }
        }
        require(false);
    }

    void register_scheme (const ResourceScheme* scheme) {
        usize h = hash(scheme->name);
        for (auto& s : schemes) {
            if (s.hash == h && s.value->name == scheme->name) {
                require(false);
            }
        }
        schemes.emplace_back(h, scheme);
    }

    void unregister_scheme (const ResourceScheme* scheme) {
        usize h = hash(scheme->name);
        for (auto& s : schemes) {
            if (s.hash == h && s.value->name == scheme->name) {
                schemes.erase(&s);
                return;
            }
        }
    }

    const ResourceScheme* require_scheme (const IRI& name) {
        Str scheme = name.scheme();
        if (schemes) {
            usize h = hash(scheme);
            for (auto& s : schemes) {
                if (s.hash == h && s.value->name == scheme) {
                    return s.value;
                }
            }
        }
        else {
            if (scheme == "file") return &default_scheme;
        }
        raise(e_ResourceSchemeNotFound, name.spec());
    }
};


inline Universe& universe () {
     // The memory leak detector flags the universe's resources as leaked,
     // because at program close, the array of resource refs is destroyed
     // without destroying the resources.  How do we solve that?  By leaking the
     // array too!
    static Indestructible<Universe> r;
    return *r;
}

} // namespace ayu::in

