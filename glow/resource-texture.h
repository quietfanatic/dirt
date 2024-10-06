// A super basic image type loaded through SDL_image and sent directly to an
// OpenGL texture.  The image pixels do not stay in CPU memory.  Does not
// support mipmaps (please set filtering to a non-mipmap mode).

#pragma once

#include "../iri/iri.h"
#include "../uni/common.h"
#include "gl.h"
#include "texture.h"

namespace glow {

struct ResourceTexture : Texture {
    iri::IRI source;
    ResourceTexture (u32 target = GL_TEXTURE_2D);
    ~ResourceTexture ();
    void load ();
};

constexpr uni::ErrorCode e_ResourceTextureLoadFailed = "glow::e_ResourceTextureLoadFailed";

} // namespace glow
