#pragma once
#include "image.h"
#include "objects.h"
#include "image-transform.h"

namespace glow {

 // Represents a texture loaded from an image.  Does not automatically support mipmaps.
struct ImageTexture : Texture {
    SubImage source;
    ReplaceColor replace_color;
    BVec flip;
    uint internalformat;
     // Supported targets: GL_TEXTURE_2D, GL_TEXTURE_1D_ARRAY, GL_TEXTURE_RECTANGLE
    explicit ImageTexture (
        uint target = 0,
        const SubImage& subimage = {},
        BVec flip = {false, true},  // Flip vertically by default
        uint internalformat = 0x1908  // GL_RGBA
    ) :
        Texture(target), source(subimage), flip(flip), internalformat(internalformat)
    {
        init();
    }
     // (Re)uploads texture if target is not 0.
    void init ();
};

} // glow
