 // The "Universe" manages the set of loaded resources and related global data.

#pragma once

#include <memory>
#include "../common.h"
#include "resource.h"
#include "scheme.h"

namespace ayu::in {

struct ResourceData : Resource {
    ResourceState state = RS::Unloaded;
     // These are only used during reachability scanning, but we have extra room
     // for them here.
    bool root;
    bool reachable;
     // This is also only used during reachability scanning, and we don't have
     // extra room for it, but storing it externally would require using an
     // unordered_map (to use a UniqueArray, we need an integer index, but
     // that's what this itself is).
    u32 node_id;
    IRI name;
    AnyVal value {};
    ResourceData (const IRI& n) : name(n) { }
};

struct Universe {
     // The memory leak detector flags the resources as leaked, because at
     // program close, this array is destroyed without destroying the resources.
     // TODO to prevent this (possibly by leaking the array too).
    UniqueArray<Hashed<ResourceRef>> resources;
    UniqueArray<Hashed<const ResourceScheme*>> schemes;
    UniqueArray<AnyPtr> globals;

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
        usize h = hash(scheme->scheme_name);
        for (auto& s : schemes) {
            if (s.hash == h && s.value->scheme_name == scheme->scheme_name) {
                raise(e_ResourceSchemeNameDuplicate, scheme->scheme_name);
            }
        }
        schemes.emplace_back(h, scheme);
    }
    const ResourceScheme* require_scheme (const IRI& name) {
        Str scheme = name.scheme();
        usize h = hash(scheme);
        for (auto& s : schemes) {
            if (s.hash == h && s.value->scheme_name == scheme) {
                return s.value;
            }
        }
        raise(e_ResourceSchemeNotFound, name.spec());
    }
    void unregister_scheme (const ResourceScheme* scheme) {
        usize h = hash(scheme->scheme_name);
        for (auto& s : schemes) {
            if (s.hash == h && s.value->scheme_name == scheme->scheme_name) {
                schemes.erase(&s);
                return;
            }
        }
    }
};


inline Universe& universe () {
    static Universe r;
    return r;
}

} // namespace ayu::in

