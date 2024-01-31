#pragma once
#include "resource.h"

namespace ayu {

 // A SharedResource that lets you know at compile-time what type it is.
struct TypedResource : SharedResource {
};

} // ayu
