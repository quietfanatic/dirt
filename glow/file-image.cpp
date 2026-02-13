#include "file-image.h"

#include "../geo/values.h"
#include "../ayu/resources/resource.h"
#include "load-image.h"

namespace glow {

FileImage::FileImage (const AnyString& s, Slice<u8> b) :
    source(s),
    storage(load_image_from_blob(b, s))
{ }

void FileImage::load () {
    if (storage) return;
    storage = load_image_from_file(source);
}

void FileImage::trim () {
    if (storage) {
        free(storage.pixels);
        storage.pixels = null;
    }
}

FileImage::operator ImageRef () {
    load();
    return ImageRef(storage);
}

bool FileImageExtension::accepts_type (ayu::Type type) {
    return type == ayu::Type::For<FileImage>();
}

void FileImageExtension::from_blob (
    ayu::AnyVal& value, Slice<u8> blob,
    ayu::ResourceRef res, ayu::ResourceScheme* scheme
) {
    scheme->validate_type(ayu::Type::For<FileImage>());
    auto path = ayu::resource_filepath(res->name());
    expect(!value);
    value = ayu::AnyVal::make<FileImage>(path, blob);
};

UniqueArray<u8> FileImageExtension::to_blob (
    const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions
) {
    raise(e_General, "Saving image files is NYI");
}

} using namespace glow;

 // You can't deserialize this in item_from_tree.  Instead you should use
 // FileImageExtension.  The main reason we have this description is just to
 // allow dynamic upcasting to glow::Image.
AYU_DESCRIBE(glow::FileImage,
    attrs(
        attr("glow::Image", base<glow::Image>(), include),
        attr("source", member(&FileImage::source, readonly))
    )
)
