#pragma once

#include "common.h"
#include "../geo/vec.h"

namespace glow {

 // A texture in video memory.
 // glGenTextures will be called on construction and glDeleteTextures on
 //  destruction.
struct Texture {
     // Specifies what kind of texture this is.  GL_TEXTURE_*.
     // If 0, texture won't actually be created.
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

    const u32 id = 0;
    operator u32 () const { return id; }

     // Uses glGetTexLevelParameter
     // Returns {0, 0} if this texture (level) has not been initialized
    geo::IVec size (i32 level = 0);
     // Returns 0 if this texture (level) has not been initialized
     // I believe this can return a maximum of 256 (double precision RGBA)
    i32 bpp (i32 level = 0);
};

} // namespace glow
