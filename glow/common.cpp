#include "common.h"

#include <cstdlib>
#include <iostream>
#include <SDL2/SDL_error.h>
#include "../uni/strings.h"
#include "../uni/io.h"
#include "gl.h"

namespace glow {

void init () noexcept {
    init_gl_functions();
}

[[gnu::cold]]
void requirement_failed_sdl (std::source_location loc) try {
    raise(e_SDLRequirementFailed, "ERROR: require_sdl() failed");
} catch (Error& e) {
    e.add_tag("std::source_location", show_source_location(loc));
     // SDL doesn't have numeric error codes
    e.rethrow_with_tag("SDL_GetError()", SDL_GetError());
}

} using namespace glow;
