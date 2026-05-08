#pragma once

#include <SDL2/SDL.h>
#include "../../ayu/resources/scheme.h"
#include "../../geo/vec.h"
#include "../../wind/window.h"
#include "../common.h"
#include "../gl.h"
#include "../image.h"

namespace glow::test {

struct TestEnvironment {
    geo::IVec size;
    ayu::FolderResourceScheme test_scheme;
    wind::Window window;
    TestEnvironment (geo::IVec size = {120, 120}) :
        size(size),
        test_scheme(
            "test",
            []{
                char* base = require_sdl(SDL_GetBasePath());
                auto folder = cat(base, "res/dirt/glow/test");
                SDL_free(base);
                return folder;
            }()
        ),
        window("Test window", size, wind::GLAttributes{.alpha = 8})
    {
         // Some gl drivers won't render to hidden windows, so do our best to hide
         // the window manually
        SDL_MinimizeWindow(window);
        SDL_ShowWindow(window);
        SDL_MinimizeWindow(window);
        glow::init();
         // Make sure we got a window of the correct size
        int w; int h;
        SDL_GetWindowSize(window, &w, &h);
        require(w == size.x && h == size.y);
    }
    ~TestEnvironment () { }

    UniqueImage read_pixels () {
        UniqueImage r (size);
        glFinish();
        glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, r.pixels);
        return r;
    }
};

} // namespace glow::test
