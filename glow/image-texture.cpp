#include "image-texture.h"

#include "../ayu/reflection/describe.h"
#include "gl.h"

namespace glow {

[[noreturn, gnu::cold]]
void raise_SubImageBoundsNotProper (const SubImage& self) {
    raise(e_SubImageBoundsNotProper, ayu::show(&self.bounds));
}

[[noreturn, gnu::cold]]
void raise_SubImageOutOfBounds (const SubImage& self, IVec size) {
    raise(e_SubImageOutOfBounds, cat(
        "SubImage is out of bounds of image at ", ayu::show(self.source),
        "\n    Image size: ", ayu::show(&size),
        "\n    SubImage bounds: ", ayu::show(&self.bounds)
    ));
}

void SubImage::validate () {
    if (bounds != GINF) {
        if (!proper(bounds)) {
            raise_SubImageBoundsNotProper(*this);
        }
        if (source) {
            auto data = source->get();
            if (!contains(data.bounds(), bounds)) {
                raise_SubImageOutOfBounds(*this, data.size);
            }
        }
    }
}

ImageTexture::ImageTexture () : Texture(GL_TEXTURE_2D) {
    glBindTexture(target, id);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

PixelTexture::PixelTexture () {
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

void ImageTexture::init () {
    if (target && source) {
        require(target == GL_TEXTURE_2D
            || target == GL_TEXTURE_1D_ARRAY
            || target == GL_TEXTURE_RECTANGLE
        );
        glBindTexture(target, id);
        ImageView data = source;
        UniqueImage processed (data.size);
        for (i32 y = 0; y < data.size.y; y++)
        for (i32 x = 0; x < data.size.x; x++) {
            processed.pixels[y * processed.size.x + x] =
                data.pixels[(data.size.y - y - 1) * data.stride + x];
        }
        glTexImage2D(
            target,
            0, // level
            GL_RGBA8,
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

AYU_DESCRIBE(glow::SubImage,
    attrs(
        attr("source", &SubImage::source),
        attr("bounds", &SubImage::bounds, optional)
    ),
    init([](SubImage& v){ v.validate(); })
)

AYU_DESCRIBE(glow::ImageTexture,
    attrs(
         // TODO: figure out how to make this optional without regenning texture
        attr("Texture", base<Texture>(), include),
        attr("SubImage", &ImageTexture::source, include),
        attr("flip", &ImageTexture::flip, optional)
    ),
    init([](ImageTexture& v){ v.init(); })
)

AYU_DESCRIBE(glow::PixelTexture,
    delegate(base<ImageTexture>())
)

