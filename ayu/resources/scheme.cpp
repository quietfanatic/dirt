#include "scheme.h"

#include "universe.private.h"

namespace ayu {
using namespace in;

void ResourceScheme::activate () const {
    if (!iri::scheme_valid(scheme_name)) {
        raise(e_ResourceSchemeNameInvalid, scheme_name);
    }
    universe().register_scheme(this);
}
void ResourceScheme::deactivate () const noexcept {
    universe().unregister_scheme(this);
}

} using namespace ayu;

