#include "image-texture.h"

#include "../ayu/reflection/describe.h"
#include "gl.h"

namespace glow {

void ImageTexture::init () {
    if (target && source) {
        require(target == GL_TEXTURE_2D
            || target == GL_TEXTURE_1D_ARRAY
            || target == GL_TEXTURE_RECTANGLE
        );
        ImageRef data = source;
        UniqueImage processed = replace_color.apply(data, flip);
        glBindTexture(target, id);
        glTexImage2D(
            target,
            0, // level
            internalformat,
            processed.size.x,
            processed.size.y,
            0, // border
            GL_RGBA, // format
            GL_UNSIGNED_BYTE, // type
            processed.pixels
        );
    }
}

} using namespace glow;

AYU_DESCRIBE(glow::ImageTexture,
    attrs(
         // TODO: figure out how to make this optional without regenning texture
        attr("Texture", base<Texture>(), include),
        attr("SubImage", &ImageTexture::source, include),
        attr("replace_color", &ImageTexture::replace_color, optional),
        attr("flip", &ImageTexture::flip, optional),
        attr("internalformat", &ImageTexture::internalformat, optional)
    ),
    init([](ImageTexture& v){ v.init(); })
)

