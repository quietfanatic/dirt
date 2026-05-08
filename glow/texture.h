#pragma once

#include "common.h"
#include "../geo/vec.h"

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
    geo::IVec size (i32 level = 0);
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

} // namespace glow
