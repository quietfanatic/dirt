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
void requirement_failed_sdl (std::source_location loc) noexcept {
    warn_utf8(uni::cat(
        "ERROR: require_sdl() failed at", loc.file_name(),
        ':', loc.line(), "\n       in ", loc.function_name(),
        "\n       SDL_GetError() == ", SDL_GetError()
    ));
    std::abort();
}

} using namespace glow;
