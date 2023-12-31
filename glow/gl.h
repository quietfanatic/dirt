#pragma once

#include "common.h"

namespace glow {
    void register_gl_function (void*, const char*) noexcept;
    void init_gl_functions () noexcept;

    constexpr uni::ErrorCode e_GLError = "glow::e_GLError";

     // TODO: warn instead of throwing
    void throw_on_glGetError (
        const char* function_name,
        std::source_location = std::source_location::current()
    );
}

 // Build GL API

#define DECLARE_GL_FUNCTION(name, Ret, params) \
template <int = 0> \
Ret(APIENTRY* p_##name )params noexcept = \
    (glow::register_gl_function(&p_##name<>, #name), nullptr);

#ifdef NDEBUG

 // TODO: change to not require p_
#define USE_GL_FUNCTION(p_name) p_name<>

#else
 // For debug, check glGetError after every call.

#include <type_traits>

#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif

namespace glow {
    template <class Ret, class... Args>
    auto checked_gl_function(
        Ret(APIENTRY* p )(Args...),
        const char* fname,
        std::source_location srcloc = std::source_location::current()
    ) {
         // Bad hack: Add 2 to fname to remove the p_
        return [=](Args... args) -> Ret {
            if constexpr (std::is_void_v<Ret>) {
                p(args...);
                throw_on_glGetError(fname + 2, srcloc);
            }
            else {
                Ret r = p(args...);
                throw_on_glGetError(fname + 2, srcloc);
                return r;
            }
        };
    }
}

#define USE_GL_FUNCTION(p_name) glow::checked_gl_function(p_name<>, #p_name)

#endif

#include "../gl_api/gl_api.h"
