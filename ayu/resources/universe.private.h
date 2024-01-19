 // The "Universe" manages the set of loaded resources and related global data.

#pragma once

#include <memory>
#include <unordered_map>
#include "../common.h"
#include "resource.h"
#include "scheme.h"

namespace ayu::in {

struct ResourceData {
    IRI name;
    Dynamic value {};
    Dynamic old_value {};  // Used when reloading
    ResourceState state = UNLOADED;
    uint32 purpose_count = 0;
};

struct Universe {
     // The Str here must refer to the resource's name.spec().
    std::unordered_map<Str, std::unique_ptr<ResourceData>> resources;
    std::unordered_map<AnyString, const ResourceScheme*> schemes;
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

