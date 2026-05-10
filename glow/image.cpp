#include "image.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/to-tree.h"

namespace glow {

void blit (const ImageView&__restrict dst, const ImageView&__restrict src) noexcept {
    RGBA8*__restrict out = dst.pixels;
    RGBA8*__restrict in = src.pixels;
    expect(dst.size == src.size);
    expect(dst.size.x >= 0 && dst.size.y >= 0);
    if (dst.contiguous() & src.contiguous()) {
        std::memcpy(out, in, area(dst.size) * sizeof(RGBA8));
    }
    else for (i32 y = 0; y < dst.size.y; y++) {
        RGBA8* o = out;
        RGBA8* i = in;
        for (i32 x = 0; x < dst.size.x; x++) {
            *o++ = *i++;
        }
        out += dst.stride;
        in += src.stride;
    }
}

UniqueImage image_from_blob (Slice<u8> blob, Str filepath) {
    return image_from_blob_qoi(blob, filepath);
}

UniqueImage image_from_file (SharedString filepath) {
#ifdef GLOW_USE_SAIL
    return image_from_file_sail(move(filepath));
#else
    return image_from_file_qoi(move(filepath));
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
    auto img = image_from_blob(blob);
    expect(!value);
    value = ayu::AnyVal::make<FileImage>(path, move(img));
};

UniqueArray<u8> FileImageExtension::to_blob (
    const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions
) {
    raise(e_General, "Saving image files is NYI");
}

struct UniqueImagePixelsProxy : UniqueImage { };

} using namespace glow;

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

 // Consider using FileImageExtension instead of storing this in .ayu data.
AYU_DESCRIBE(glow::FileImage,
    attrs(
        attr("glow::UniqueImage", base<glow::UniqueImage>(), include),
        attr("filepath", member(&FileImage::filepath, readonly), optional)
    )
)
