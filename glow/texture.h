#pragma once

#include "common.h"
#include "image.h"
#include "../geo/rect.h"

namespace glow {

 // A texture in video memory.
 // glGenTextures will be called on construction and glDeleteTextures on
 // destruction.  Does NOT call glBindTexture on construction.
struct Texture {
     // Specifies what kind of texture this is.  GL_TEXTURE_*.
     // If 0, texture won't actually be created.  Serialized.
    const u32 target;

    explicit Texture (u32 target = 0);

    Texture (Texture&& o) : target(o.target), id(o.id) {
        const_cast<u32&>(o.id) = 0;
    }
    ~Texture ();
    Texture& operator= (Texture&& o) {
        this->~Texture();
        return *new (this) Texture(move(o));
    }

    const u32 id = 0; // Not serialized.
    operator u32 () const { return id; }

    ///// ACCESSORS
     // Most properties should be accessed via gl_GetTexParameter and
     // gl_GetTexLevelParameter.  For your convenience, here are some compound
     // properties that would otherwise take several lines to access.

     // Returns {0, 0} if this texture (level) has not been initialized
    IVec size (i32 level = 0);
     // Returns 0 if this texture (level) has not been initialized
     // I believe this can return a maximum of 256 (double precision RGBA)
    i32 bpp (i32 level = 0);
};

 // Load texture from an image.  The image must be contiguous (stride ==
 // size.x).  TODO: see if we can relax that requirement.
void texture_from_image (u32 target, const ImageView& img) noexcept;

 // Load straight from a file to an OpenGL texture.  When using SAIL, Supports a
 // few more efficient internal formats, but only up to 8bit color.  Call
 // glBindTexture first.
void texture_from_file (u32 target, SharedString filepath);
void texture_from_file_qoi (u32 target, SharedString filepath);
#ifdef GLOW_USE_SAIL
void texture_from_file_sail (u32 target, SharedString filepath);
#endif

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

} // namespace glow
