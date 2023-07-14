#include "../resource-scheme.h"

#include "../errors.h"
#include "universe-private.h"

namespace ayu {
using namespace in;

void ResourceScheme::activate () const {
    auto& schemes = universe().schemes;
     // Easiest way to validate is just try creating an IRI
    if (!IRI(cat(scheme_name, ":"))) {
        throw InvalidResourceScheme(scheme_name);
    }
    auto [iter, emplaced] = schemes.emplace(scheme_name, this);
    if (!emplaced) {
        throw DuplicateResourceScheme(scheme_name);
    }
}
void ResourceScheme::deactivate () const {
    auto& schemes = universe().schemes;
    schemes.erase(scheme_name);
}

} using namespace ayu;

