#pragma once

#include "common.h"

#ifdef GLOW_TRACE_GL
#include "../uni/io.h"
#include "../uni/strings.h"
#endif

#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif

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

#if __GNUC__
#define DECLARE_GL_FUNCTION(name, Ret, params) \
template <int = 0> \
[[gnu::constructor]] void register_p_##name (); \
template <int = 0> \
Ret(APIENTRY* p_##name )params noexcept = (&register_p_##name<>, nullptr); \
template <int> \
[[gnu::constructor]] void register_p_##name () { \
    glow::register_gl_function(&p_##name<>, #name); \
}
#else
#define DECLARE_GL_FUNCTION(name, Ret, params) \
template <int = 0> \
Ret(APIENTRY* p_##name )params noexcept = \
    (glow::register_gl_function(&p_##name<>, #name), nullptr);
#endif

#ifdef NDEBUG

#define USE_GL_FUNCTION(name) p_##name<>

#else
 // For debug, check glGetError after every call.

#include <type_traits>

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
#ifdef GLOW_TRACE_GL
                warn_utf8(cat("void ", fname, cat(" ", args)..., "\n"));
#endif
                throw_on_glGetError(fname + 2, srcloc);
            }
            else {
                Ret r = p(args...);
#ifdef GLOW_TRACE_GL
                warn_utf8(cat(r, " ", fname, cat(" ", args)..., "\n"));
#endif
                throw_on_glGetError(fname + 2, srcloc);
                return r;
            }
        };
    }
}

#define USE_GL_FUNCTION(name) glow::checked_gl_function(p_##name<>, #name)

#endif

#include "../gl_api/gl_api.h"
