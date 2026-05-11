#include "image.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/to-tree.h"
#include "../uni/io.h"

namespace glow {

 // This doesn't need to be all that optimized.
void blit (const ImageView& dst, const ImageView& src) noexcept {
    expect(dst.size.x >= 0 && dst.size.y >= 0);
    expect(dst.size == src.size);
    if (dst.contiguous() & src.contiguous()) {
        std::memcpy(dst.pixels, src.pixels, area(dst.size) * sizeof(RGBA8));
    }
    else {
        auto o = dst.pixels;
        auto i = src.pixels;
        auto oe = dst.pixels + dst.stride * dst.size.y;
        auto s = dst.size.x * sizeof(RGBA8);
        auto ot = dst.stride;
        auto it = src.stride;
        while (o < oe) {
            std::memcpy(o, i, s);
            o += ot;
            i += it;
        }
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

UniqueImage image_from_file_qoi (SharedString filepath) {
    auto blob = blob_from_file(filepath);
    return image_from_blob_qoi(blob, move(filepath));
}

UniqueArray<u8> image_to_blob (const ImageView& img, Str filepath) {
    return image_to_blob_qoi(img, filepath);
}

void image_to_file (const ImageView& img, SharedString filepath) {
    return image_to_file_qoi(img, move(filepath));
}

void image_to_file_qoi (const ImageView& img, SharedString filepath) {
    auto blob = image_to_blob_qoi(img, filepath);
    blob_to_file(blob, move(filepath));
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
