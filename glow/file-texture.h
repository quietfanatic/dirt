// A super basic image type loaded from a file and sent directly to an OpenGL
// texture.  The image pixels do not stay in CPU memory.  Not serializable.
// Does not support mipmaps (please set filtering to a non-mipmap mode).

#pragma once

#include "../geo/vec.h"
#include "../uni/common.h"
#include "gl.h"
#include "texture.h"

namespace glow {

struct FileTexture : Texture {
    FileTexture (AnyString filename, u32 target = GL_TEXTURE_2D);
    ~FileTexture ();
};

} // namespace glow
