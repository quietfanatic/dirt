#include "resource-image.h"

#include "../geo/values.h"
#include "../ayu/resources/resource.h"
#include "load-image.h"

namespace glow {

void ResourceImage::load () {
    if (storage) return;
    storage = load_image_from_file(ayu::resource_filename(source).c_str());
}

void ResourceImage::trim () {
    if (storage) {
        free(storage.pixels);
        storage.pixels = null;
    }
}

ResourceImage::operator ImageRef () {
    load();
    return ImageRef(storage);
}

} using namespace glow;

AYU_DESCRIBE(glow::ResourceImage,
    attrs(
        attr("glow::Image", base<glow::Image>(), include),
        attr("source", &ResourceImage::source)
    ),
    init<&ResourceImage::trim>(GINF)
)
