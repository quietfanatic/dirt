#include "scheme.h"

#include "universe.private.h"

namespace ayu {
using namespace in;

void ResourceScheme::activate () const {
     // Easiest way to validate is just try creating an IRI
     // TODO: this is obviously incorrect what was I thinking
    if (!IRI(cat(scheme_name, ":"))) {
        raise(e_ResourceSchemeNameInvalid, scheme_name);
    }
    universe().register_scheme(this);
}
void ResourceScheme::deactivate () const noexcept {
    universe().unregister_scheme(this);
}

} using namespace ayu;

