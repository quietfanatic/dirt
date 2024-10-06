#include "resource-texture.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/resources/resource.h"
#include "../uni/io.h"

#include <SDL2/SDL_image.h>
#include "../glow/gl.h"

#ifdef GLOW_PROFILING
#include "../uni/time.h"
#endif

namespace glow {

ResourceTexture::ResourceTexture (ui32 target) : Texture(target) {
    glBindTexture(target, id);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void ResourceTexture::load () {
    static const bool init [[maybe_unused]] = []{
         // TODO: Get a newer version of SDL_image that supports avif
        auto flags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF
                   | IMG_INIT_WEBP;
        IMG_Init(flags);
        return true;
    }();
#ifdef GLOW_PROFILING
    double time0 = uni::now();
#endif
    SDL_Surface* surf = IMG_Load(
        ayu::resource_filename(source).c_str()
    );
    if (!surf) raise(e_ResourceTextureLoadFailed, SDL_GetError());
#ifdef GLOW_PROFILING
    double time1 = uni::now();
    double time2 = time1;
#endif
     // Translate SDL formats into OpenGL formats
     // TODO: select more efficient internal format
    GLenum internal_format;
    GLenum format;
    GLenum type;
     // I think these are correct wrt endianness
    switch (surf->format->format) {
         // I've never seen IMG_Load give me image formats besides these, so
         // we'll go ahead and convert everything else
        case SDL_PIXELFORMAT_RGB24:
            internal_format = GL_RGB8;
            format = GL_RGB;
            type = GL_UNSIGNED_BYTE;
            break;
        case SDL_PIXELFORMAT_RGBA32:
            internal_format = GL_RGBA8;
            format = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            break;
        default: {
             // Nontrivial format, so ask SDL to convert
            uni::warn_utf8(uni::cat("Weird image format, converting ", source.spec(), "\n"));
            ui32 sdl_format;
            if (SDL_ISPIXELFORMAT_ALPHA(surf->format->format)) {
                sdl_format = SDL_PIXELFORMAT_RGBA32;
                internal_format = GL_RGBA8;
                format = GL_RGBA;
                type = GL_UNSIGNED_BYTE;
            }
            else {
                sdl_format = SDL_PIXELFORMAT_RGB24;
                internal_format = GL_RGB8;
                format = GL_RGB;
                type = GL_UNSIGNED_BYTE;
            }
            SDL_Surface* new_surf = require_sdl(
                SDL_ConvertSurfaceFormat(surf, sdl_format, 0)
            );
            SDL_FreeSurface(surf);
            surf = new_surf;
#ifdef GLOW_PROFILING
            time2 = uni::now();
#endif
            break;
        }
    }
     // Detect greyscale images and unused alpha channels
    auto pixels = reinterpret_cast<ui8*>(surf->pixels);
    if (surf->format->format == SDL_PIXELFORMAT_RGB24) {
        bool greyscale = true;
        for (i32 y = 0; y < surf->h; y++)
        for (i32 x = 0; x < surf->w; x++) {
            ui8 r = pixels[y * surf->pitch + x*3];
            ui8 g = pixels[y * surf->pitch + x*3 + 1];
            ui8 b = pixels[y * surf->pitch + x*3 + 2];
            if (r != g || g != b) {
                greyscale = false;
                goto done_24;
            }
        }
        done_24:;
        if (greyscale) internal_format = GL_R8;
    }
    else if (surf->format->format == SDL_PIXELFORMAT_RGBA32) {
        bool greyscale = true;
        bool unused_alpha = true;
        for (i32 y = 0; y < surf->h; y++)
        for (i32 x = 0; x < surf->w; x++) {
            ui8 r = pixels[y * surf->pitch + x*4];
            ui8 g = pixels[y * surf->pitch + x*4 + 1];
            ui8 b = pixels[y * surf->pitch + x*4 + 2];
            ui8 a = pixels[y * surf->pitch + x*4 + 3];
            if (r != g || g != b) {
                greyscale = false;
                if (!unused_alpha) goto done_32;
            }
            if (a != 255) {
                unused_alpha = false;
                if (!greyscale) goto done_32;
            }
        }
        done_32:;
        if (greyscale && unused_alpha) internal_format = GL_R8;
        else if (greyscale) internal_format = GL_RG8; // G -> A
        else if (unused_alpha) internal_format = GL_RGB8;
    }
#ifdef GLOW_PROFILING
    double time3 = uni::now();
#endif
     // Now upload texture
    require(surf->w > 0 && surf->h > 0);
    glBindTexture(target, id);
    glTexImage2D(
        target, 0, internal_format,
        surf->w, surf->h, 0, format, type, surf->pixels
    );
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
     // Make sure the right channels go to the right colors
    if (internal_format == GL_R8) {
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, GL_RED);
    }
    else if (internal_format == GL_RG8) {
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
    }
#ifdef GLOW_PROFILING
    double time4 = uni::now();
    i64 mem = surf->w * surf->h;
    if (internal_format == GL_RG8) mem *= 2;
    else if (internal_format == GL_RGB8) mem *= 3;
    else if (internal_format == GL_RGBA8) mem *= 4;
    ayu::dump(source.spec(), time1 - time0, time2 - time1, time3 - time2, time4 - time3, mem);
#endif
    SDL_FreeSurface(surf);
}

ResourceTexture::~ResourceTexture () { }

} using namespace glow;

AYU_DESCRIBE(glow::ResourceTexture,
    attrs(
        attr("glow::Texture", base<Texture>(), include),
        attr("source", &ResourceTexture::source)
    ),
    init<&ResourceTexture::load>()
)

#ifndef TAP_DISABLE_TESTS
#include "../ayu/resources/resource.h"
#include "../tap/tap.h"
#include "colors.h"
#include "test-environment.h"

static tap::TestSet tests ("dirt/glow/resource-texture", []{
    using namespace tap;
    using namespace geo;

    TestEnvironment env;

    ResourceTexture tex;
    tex.source = iri::IRI("test:/image.png");
    tex.load();
    auto size = tex.size();
    is(size, IVec{7, 5}, "Created texture has correct size");

    UniqueArray<RGBA8> got_pixels (area(size));
    glGetTexImage(tex.target, 0, GL_RGBA, GL_UNSIGNED_BYTE, got_pixels.data());
    is(got_pixels[10], RGBA8(0x2674dbff), "Created texture has correct content");

    done_testing();
});

#endif
