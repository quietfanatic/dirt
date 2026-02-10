#include "anyptr.h"
#include "link.h"
#include "describe.h"

namespace ayu {
} using namespace ayu;

 // Can't short-circuit this delegate because the resource tracker specifically
 // checks for Links and not AnyPtrs.  This could be fixed eventually.
AYU_DESCRIBE(ayu::AnyPtr,
    delegate(assignable<Link>())
);
