#include "anyptr.h"

#include "describe.h"
#include "reference.h"

namespace ayu {
} using namespace ayu;

AYU_DESCRIBE(ayu::AnyPtr,
    delegate(assignable<Reference>())
);
