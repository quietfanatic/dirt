#pragma once
#include "resource.h"

#include "../../iri/iri.h"

namespace ayu::in {

struct ResourceData : Resource {
    uint32 purpose_count = 0;
    IRI name;
    Dynamic value {};
    ResourceState state = RS::Unloaded;
    ResourceData (const IRI& n) : name(n) { }
};

void load_under_purpose (ResourceRef);

} // ayu::in
