#include "scheme.h"
#include "universe.private.h"

namespace ayu {
using namespace in;

void ResourceScheme::activate () const {
    require(iri::scheme_canonical(name));
    universe().register_scheme(this);
}
void ResourceScheme::deactivate () const noexcept {
    universe().unregister_scheme(this);
}

} using namespace ayu;

