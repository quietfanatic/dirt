#pragma once
#include "image.h"
#include "texture.h"

namespace glow {

 // Reference type that refers to a portion of another image.  This is not
 // considered an ImageSource.  TODO: fold into ImageTexture
struct SubImage {
     // Image that is being referenced.
    ImageSource* source = null;
     // Area of the subimage in pixels.  Coordinates refer to the corners
     // between pixels, not the pixels themselves.  As a special case, GINF
     // refers to the entire image.  Otherwise, cannot have negative width or
     // height and cannot be outside the bounds of the image.
    IRect bounds = GINF;

     // Will throw if bounds is outside the image or is not proper.
     // Can't check if the bounds or image size is changed later.
    void validate ();

    constexpr SubImage () { }
    SubImage (ImageSource* s, const IRect& b = GINF) :
        source(s), bounds(b)
    { validate(); }

    constexpr explicit operator bool () { return source; }

    operator ImageView () {
        auto r = source->get();
        if (bounds != GINF) {
            require(contains(r.bounds(), bounds));
            r.size = geo::size(bounds);
            r.pixels += bounds.b * r.stride + bounds.l;
        }
        return r;
    }
};

 // Represents a texture loaded from an image.  Does not support mipmaps.
 // WARNING: Do not provide a target when deserializing unless you also provide
 // a filter mode.  I need to fix the problems around texture target
 // deserialization.
struct ImageTexture : Texture {
    SubImage source;
    BVec flip = {false, true}; // Flip vertically by default
    ImageTexture ();
    void init ();
};

 // An image texture that defaults to GL_NEAREST filtering
struct PixelTexture : ImageTexture {
    PixelTexture ();
};

} // glow
