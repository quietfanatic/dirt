#pragma once
#include "resource.h"

#include "../../iri/iri.h"

namespace ayu::in {

struct ResourceData {
    IRI name;
    Dynamic value {};
    ResourceState state = RS::Unloaded;
    uint32 purpose_count = 0;
};

void load_under_purpose (Resource);

} // ayu::in
