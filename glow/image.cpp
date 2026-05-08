#include "image.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/to-tree.h"

namespace glow {

UniqueImage image_from_blob (Slice<u8> blob, Str filepath) {
    return image_from_blob_qoi(blob, filepath);
}

UniqueImage image_from_file (SharedString filepath) {
#ifdef GLOW_USE_SAIL
    return image_from_file_sail(filepath);
#else
    return image_from_file_qoi(filepath);
#endif
}

bool FileImageExtension::accepts_type (ayu::Type type) {
    return type == ayu::Type::of<FileImage>();
}

void FileImageExtension::from_blob (
    ayu::AnyVal& value, Slice<u8> blob,
    ayu::ResourceRef res, ayu::ResourceScheme* scheme
) {
    scheme->validate_type(ayu::Type::of<FileImage>());
    auto path = ayu::resource_filepath(res->name());
    expect(!value);
    value = ayu::AnyVal::make<FileImage>(path, blob);
};

UniqueArray<u8> FileImageExtension::to_blob (
    const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions
) {
    raise(e_General, "Saving image files is NYI");
}

struct UniqueImagePixelsProxy : UniqueImage { };

} using namespace glow;

 // You can't serialize this directly (no default constructor due to pure
 // virtual methods), but it needs to have a description so it can be addressed.
AYU_DESCRIBE(glow::ImageSource,
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
        attr("glow::ImageSource", base<glow::ImageSource>(), include),
         // TODO: allocate here instead of in the proxy?
        attr("size", &UniqueImage::size),
         // TODO: reinterpreting accessor
        attr("pixels", ref_func<UniqueImagePixelsProxy>(
            [](UniqueImage& img) -> UniqueImagePixelsProxy& {
                return static_cast<UniqueImagePixelsProxy&>(img);
            }
        ))
    )
)

 // You shouldn't deserialize this in item_from_tree.  Instead you should use
 // FileImageExtension.  The main reason we have this description is just to
 // allow dynamic upcasting to glow::ImageSource.
AYU_DESCRIBE(glow::FileImage,
    attrs(
        attr("glow::ImageSource", base<glow::ImageSource>(), include),
        attr("filepath", member(&FileImage::filepath, readonly))
    )
)
