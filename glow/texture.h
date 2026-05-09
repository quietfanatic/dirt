#pragma once

#include "common.h"
#include "image.h"
#include "../geo/rect.h"

namespace glow {

 // A texture in video memory.
 // On construction, calls glGenTextures and glBindTexture.
 // On destruction, calls glDeleteTextures.
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

 // Load texture from an image.  Must do an extra copy unless the image is fully
 // contiguous.  Does not automatically flipy, so if you call this on an image
 // loaded from a file, the texture will be upside-down.
void texture_from_image (u32 target, const ImageView& img) noexcept;

 // Load straight from a file to an OpenGL texture.  When using SAIL, Supports a
 // few more efficient internal formats, but only up to 8bit color.  Call
 // glBindTexture first.
void texture_from_file (u32 target, SharedString filepath);
void texture_from_file_qoi (u32 target, SharedString filepath);
#ifdef GLOW_USE_SAIL
void texture_from_file_sail (u32 target, SharedString filepath);
#endif

 // Represents a texture loaded from an image.  Does not support mipmaps.
 // WARNING: Do not provide a target when deserializing unless you also provide
 // a filter mode.  I need to fix the problems around texture target
 // deserialization.
struct ImageTexture : Texture {
    FileImage* source = null;
     // 0 means the entire image; otherwise must not be empty.  Must be proper.
    IRect bounds;
    BVec flip = {false, true}; // Flip vertically by default
    ImageTexture ();
    void init ();

    ImageView source_view () {
        require((!bounds || area(bounds)) && proper(bounds));
        source->load();
        require(contains(source->bounds(), bounds));
        ImageView img = *source;
        if (bounds) img = img.crop(bounds);
        return img.flip(flip);
    }
};

 // An image texture that defaults to GL_NEAREST filtering
struct PixelTexture : ImageTexture {
    PixelTexture ();
};

} // namespace glow
