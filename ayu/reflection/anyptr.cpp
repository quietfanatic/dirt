#include "anyptr.h"
#include "anyref.h"
#include "describe.h"

namespace ayu {
} using namespace ayu;

 // Can't short-circuit this delegate because the resource tracker specifically
 // checks for AnyRefs and not AnyPtrs.  This could be fixed eventually.
AYU_DESCRIBE(ayu::AnyPtr,
    delegate(assignable<AnyRef>())
);
