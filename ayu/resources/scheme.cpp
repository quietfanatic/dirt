#include "scheme.h"

#include "universe.private.h"

namespace ayu {
using namespace in;

void ResourceScheme::activate () const {
    auto& schemes = universe().schemes;
     // Easiest way to validate is just try creating an IRI
    if (!IRI(cat(scheme_name, ":"))) {
        raise(e_ResourceSchemeNameInvalid, scheme_name);
    }
    auto [iter, emplaced] = schemes.emplace(scheme_name, this);
    if (!emplaced) {
        raise(e_ResourceSchemeNameDuplicate, scheme_name);
    }
}
void ResourceScheme::deactivate () const noexcept {
    auto& schemes = universe().schemes;
    schemes.erase(scheme_name);
}

} using namespace ayu;

