 // The "Universe" manages the set of loaded resources and related global data.

#pragma once

#include <memory>
#include <unordered_map>
#include "../common.h"
#include "resource.h"
#include "scheme.h"

namespace ayu::in {

struct Universe {
     // The Str here must refer to the resource's name.spec().
     // The memory leak detector flags the resources as leaked, because at
     // program close, the unordered_map is destroyed without destroying the
     // resources.  TODO to prevent this (possibly by leaking the map too).
    std::unordered_map<Str, ResourceRef> resources;
    std::unordered_map<AnyString, const ResourceScheme*> schemes;
    UniqueArray<AnyPtr> globals;
    const ResourceScheme* require_scheme (const IRI& name) {
        Str scheme = name.scheme();
        auto iter = schemes.find(scheme);
        if (iter != schemes.end()) return iter->second;
        else raise(e_ResourceSchemeNotFound, name.spec());
    }
};

inline Universe& universe () {
    static Universe r;
    return r;
}

} // namespace ayu::in

