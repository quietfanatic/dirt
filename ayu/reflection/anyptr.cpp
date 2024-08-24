#include "anyptr.h"

#include "anyref.h"
#include "describe.h"

namespace ayu {
} using namespace ayu;

AYU_DESCRIBE(ayu::AnyPtr,
    delegate(assignable<AnyRef>())
);
