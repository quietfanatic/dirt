#include "image.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/to-tree.h"

namespace glow {

[[noreturn, gnu::cold]]
void raise_SubImageBoundsNotProper (const SubImage& self) {
    raise(e_SubImageBoundsNotProper, ayu::show(&self.bounds));
}

[[noreturn, gnu::cold]]
void raise_SubImageOutOfBounds (const SubImage& self, IVec size) {
    raise(e_SubImageOutOfBounds, cat(
        "SubImage is out of bounds of image at ", ayu::show(self.image),
        "\n    Image size: ", ayu::show(&size),
        "\n    SubImage bounds: ", ayu::show(&self.bounds)
    ));
}

void SubImage::validate () {
    if (bounds != GINF) {
        if (!proper(bounds)) {
            raise_SubImageBoundsNotProper(*this);
        }
        if (image) {
            auto data = image->Image_data();
            if (!contains(data.bounds(), bounds)) {
                raise_SubImageOutOfBounds(*this, data.size);
            }
        }
    }
}

struct UniqueImagePixelsProxy : UniqueImage { };

} using namespace glow;

 // You can't serialize this directly (no default constructor due to pure
 // virtual methods), but it needs to have a description so it can be addressed.
AYU_DESCRIBE(glow::Image,
    attrs()
)

AYU_DESCRIBE(glow::UniqueImagePixelsProxy,
     // TODO: Allow parsing hex string as an option?
    length(funcs(
        [](const UniqueImagePixelsProxy& image){
            return usize(area(image.size));
        },
        [](UniqueImagePixelsProxy& image, usize len){
            require(area(image.size) == isize(len));
            std::free(image.pixels);
            image.pixels = (RGBA8*)std::malloc(
                area(image.size) * sizeof(RGBA8)
            );
        }
    )),
    contiguous_elems([](UniqueImagePixelsProxy& image){
        return ayu::AnyPtr(image.pixels);
    })
)

AYU_DESCRIBE(glow::UniqueImage,
    attrs(
        attr("glow::Image", base<glow::Image>(), include),
         // TODO: allocate here instead of in the proxy?
        attr("size", &UniqueImage::size),
        attr("pixels", ref_func<UniqueImagePixelsProxy>(
            [](UniqueImage& img) -> UniqueImagePixelsProxy& {
                return static_cast<UniqueImagePixelsProxy&>(img);
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

