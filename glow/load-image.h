#include "image.h"

namespace glow {

 // Load image into OpenGL texture.  Supports a few more efficient internal
 // formats, but only up to 8bit color.  Call glBindTexture first.
void load_texture_from_file (u32 target, uni::AnyString filename);

 // Load image into CPU memory.  Only outputs RGBA8.
UniqueImage load_image_from_file (uni::AnyString filename);

constexpr uni::ErrorCode e_LoadImageFailed = "glow::e_LoadImageFailed";

} // glow
