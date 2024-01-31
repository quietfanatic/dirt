#include "file-image.h"

#include <SDL2/SDL_image.h>
#include "../geo/values.h"
#include "../ayu/resources/resource.h"

namespace glow {

FileImage::~FileImage () {
    if (storage) SDL_FreeSurface(storage);
}

void FileImage::load () {
    if (storage) return;
    static const bool init [[maybe_unused]] = []{
         // Only supporting PNG for now.
        auto flags = IMG_INIT_PNG;
        IMG_Init(flags);
        return true;
    }();
    storage = IMG_Load(
        ayu::resource_filename(source).c_str()
    );
    if (!storage) raise(e_FileImageLoadFailed, SDL_GetError());
    if (storage->format->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface* new_storage = require_sdl(
            SDL_ConvertSurfaceFormat(storage, SDL_PIXELFORMAT_RGBA32, 0)
        );
        SDL_FreeSurface(storage);
        storage = new_storage;
    }
    require(storage->w > 0 && storage->h > 0);
}

void FileImage::trim () {
    if (storage) {
        SDL_FreeSurface(storage);
        storage = null;
    }
}

FileImage::operator ImageRef () {
    load();
    return ImageRef(
        {storage->w, storage->h},
        storage->pitch / sizeof(RGBA8),  // Probably not necessary
        (RGBA8*)storage->pixels
    );
}

} using namespace glow;

AYU_DESCRIBE(glow::FileImage,
    attrs(
        attr("glow::Image", base<glow::Image>(), include),
        attr("source", &FileImage::source)
    ),
    init<&FileImage::trim>(-GINF)
)
