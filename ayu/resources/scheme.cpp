#include "scheme.h"
#include "universe.private.h"

namespace ayu {
using namespace in;

void ResourceScheme::activate () const {
    if (!iri::scheme_canonical(name)) {
        raise(e_ResourceSchemeNameInvalid, name);
    }
    universe().register_scheme(this);
}
void ResourceScheme::deactivate () const noexcept {
    universe().unregister_scheme(this);
}

} using namespace ayu;

