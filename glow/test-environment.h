#pragma once

#include "../ayu/resources/scheme.h"
#include "../geo/vec.h"
#include "../wind/window.h"

namespace glow {
struct UniqueImage;

struct TestEnvironment {
    geo::IVec size;
    ayu::FileResourceScheme test_scheme;
    wind::Window window;
    TestEnvironment (geo::IVec size = {120, 120});
    ~TestEnvironment ();

    UniqueImage read_pixels ();
};

} // namespace glow
