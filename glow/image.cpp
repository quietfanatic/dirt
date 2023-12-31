#include "image.h"

#include <cerrno>
#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/to-tree.h"
#include "gl.h"

namespace glow {

Image::Image (IVec s) noexcept :
    size((require(area(s) >= 0), s)), pixels(new RGBA8 [area(size)])
{ }

Image::~Image () { delete[](pixels); }

void SubImage::validate () {
    if (bounds != GINF) {
        if (!proper(bounds)) ayu::raise(e_SubImageBoundsNotProper,
            ayu::item_to_string(&bounds));
        if (image && !contains(image->bounds(), bounds)) {
            ayu::raise(e_SubImageOutOfBounds, cat(
                "SubImage is out of bounds of image at ", ayu::item_to_string(image),
                "\n    Image size: ", ayu::item_to_string(&image->size),
                "\n    SubImage bounds: ", ayu::item_to_string(&bounds)
            ));
        }
    }
}

void ImageTexture::init () {
    if (target && source) {
        require(target == GL_TEXTURE_2D
            || target == GL_TEXTURE_1D_ARRAY
            || target == GL_TEXTURE_RECTANGLE
        );
        Image processed (source.size());
        IRect ib = source.bounds != GINF ?
            source.bounds : IRect{{0, 0}, source.image->size};
        for (int y = 0; y < processed.size.y; y++)
        for (int x = 0; x < processed.size.x; x++) {
            processed[{x, y}] = source[{
                flip.x ? ib.r - x - 1 : ib.l + x,
                flip.y ? ib.t - y - 1 : ib.b + y
            }];
        }
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

struct ImagePixelsProxy : Image { };

} using namespace glow;

AYU_DESCRIBE(glow::ImagePixelsProxy,
     // TODO: Allow parsing hex string as an option?
    length(value_funcs<usize>(
        [](const ImagePixelsProxy& image){
            return usize(area(image.size));
        },
        [](ImagePixelsProxy& image, usize len){
            require(area(image.size) == isize(len));
            delete[](image.pixels);
            const_cast<RGBA8*&>(image.pixels) = new RGBA8 [area(image.size)];
        }
    )),
    elem_func([](ImagePixelsProxy& image, usize i){
        return ayu::Reference(&image.pixels[i]);
    })
)

AYU_DESCRIBE(glow::Image,
    attrs(
         // TODO: allocate here instead of in the proxy?
        attr("size", &Image::size),
        attr("pixels", ref_func<ImagePixelsProxy>(
            [](Image& img) -> ImagePixelsProxy& {
                return static_cast<ImagePixelsProxy&>(img);
            }
        ))
    )
)

AYU_DESCRIBE(glow::SubImage,
    attrs(
        attr("image", &SubImage::image),
        attr("bounds", &SubImage::bounds, optional)
    ),
    init([](SubImage& v){ v.validate(); })
)

AYU_DESCRIBE(glow::ImageTexture,
    attrs(
         // TODO: figure out how to make this optional without regenning texture
        attr("Texture", base<Texture>(), include),
        attr("SubImage", &ImageTexture::source, include),
        attr("flip", &ImageTexture::flip, optional),
        attr("internalformat", &ImageTexture::internalformat, optional)
    ),
    init([](ImageTexture& v){ v.init(); })
)

