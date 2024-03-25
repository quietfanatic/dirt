#pragma once
#include "image.h"
#include "texture.h"
#include "image-transform.h"

namespace glow {

 // Represents a texture loaded from an image.  Does not support mipmaps.
 // WARNING: Do not provide a target when deserializing unless you also provide
 // a filter mode.  I need to fix the problems around texture target
 // deserialization.
struct ImageTexture : Texture {
    SubImage source;
    ReplaceColor replace_color;
    BVec flip = {false, true}; // Flip vertically by default
    ImageTexture ();
    void init ();
};

 // An image texture that defaults to GL_NEAREST filtering
struct PixelTexture : ImageTexture {
    PixelTexture ();
};

} // glow
