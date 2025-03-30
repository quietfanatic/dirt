#include "gl.h"

#include <iomanip>
#include <sstream>
#include <SDL2/SDL_video.h>
#include "../uni/io.h"

using namespace std::literals;

namespace glow {

struct GLFunctionRegistry {
    UniqueArray<std::pair<void*, const char*>> to_init;
    bool initted = false;
};
static GLFunctionRegistry& registry () {
    static GLFunctionRegistry r;
    return r;
}

void register_gl_function (void* p, const char* name) noexcept {
    static GLFunctionRegistry& reg = registry();
    require(!reg.initted);
    reg.to_init.emplace_back(p, name);
}

void init_gl_functions () noexcept {
    static GLFunctionRegistry& reg = registry();
    if (reg.initted) return;
    reg.initted = true;

    require_sdl(!SDL_GL_LoadLibrary(NULL));

    for (auto& p : reg.to_init) {
        *reinterpret_cast<void**>(p.first) = require_sdl(SDL_GL_GetProcAddress(p.second));
    }
    reg.to_init.clear();
}

void warn_on_glGetError (
    const char* function_name,
    std::source_location srcloc
) noexcept {
    GLenum err = p_glGetError<>();
    if (err) uni::warn_utf8(cat(
        "GL error code ", err, " from ", function_name,
        " in ", srcloc.function_name(), " at ", srcloc.file_name(), ':', srcloc.line()
    ));
}

} using namespace glow;
