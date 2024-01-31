#pragma once
#include "image.h"

namespace glow {

UniqueImage copy_pixels (const ImageRef& in, BVec flip = {});

struct ReplaceColor {
    RGBA8 from;
    RGBA8 to;
    UniqueImage apply (const ImageRef& in, BVec flip = {});
};

 // TODO
//struct Palette {
//    Image* image;
//    IVec pos;
//    bool horizontal = false;
//    UniqueImage apply (const ImageRef& in, BVec flip = {});
//};

} // glow
