#include "texture.h"

#include "../ayu/reflection/describe.h"
#include "gl.h"

namespace glow {

Texture::Texture (u32 target) : target(target) {
    if (target) {
        init();
        glGenTextures(1, &const_cast<u32&>(id));
        glBindTexture(target, id);
    }
}

Texture::~Texture () {
    if (id) {
        glDeleteTextures(1, &id);
    }
}

geo::IVec Texture::size (i32 level) {
    geo::IVec r;
    glBindTexture(target, id);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &r.x);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &r.y);
    return r;
}
i32 Texture::bpp (i32 level) {
    i32 rsize, gsize, bsize, asize;
    glBindTexture(target, id);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_RED_SIZE, &rsize);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_GREEN_SIZE, &gsize);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_BLUE_SIZE, &bsize);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_ALPHA_SIZE, &asize);
    return rsize + gsize + bsize + asize;
}

enum TextureTarget { };
enum TextureWrap { };
enum TextureMagFilter { };
enum TextureMinFilter { };

} using namespace glow;

AYU_DESCRIBE(glow::TextureTarget,
    values(
        value("GL_TEXTURE_1D", TextureTarget(GL_TEXTURE_1D)),
        value("GL_TEXTURE_2D", TextureTarget(GL_TEXTURE_2D)),
        value("GL_TEXTURE_3D", TextureTarget(GL_TEXTURE_3D)),
        value("GL_TEXTURE_1D_ARRAY", TextureTarget(GL_TEXTURE_1D_ARRAY)),
        value("GL_TEXTURE_2D_ARRAY", TextureTarget(GL_TEXTURE_2D_ARRAY)),
        value("GL_TEXTURE_RECTANGLE", TextureTarget(GL_TEXTURE_RECTANGLE)),
        value("GL_TEXTURE_CUBE_MAP", TextureTarget(GL_TEXTURE_CUBE_MAP)),
        value("GL_TEXTURE_CUBE_MAP_ARRAY", TextureTarget(GL_TEXTURE_CUBE_MAP_ARRAY)),
        value("GL_TEXTURE_BUFFER", TextureTarget(GL_TEXTURE_BUFFER)),
        value("GL_TEXTURE_2D_MULTISAMPLE", TextureTarget(GL_TEXTURE_2D_MULTISAMPLE)),
        value("GL_TEXTURE_2D_MULTISAMPLE_ARRAY", TextureTarget(GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
    )
)

AYU_DESCRIBE(glow::TextureWrap,
    values(
        value("GL_CLAMP_TO_EDGE", TextureWrap(GL_CLAMP_TO_EDGE)),
        value("GL_CLAMP_TO_BORDER", TextureWrap(GL_CLAMP_TO_BORDER)),
        value("GL_MIRRORED_REPEAT", TextureWrap(GL_MIRRORED_REPEAT)),
        value("GL_REPEAT", TextureWrap(GL_REPEAT)),
        value("GL_MIRROR_CLAMP_TO_EDGE", TextureWrap(GL_MIRROR_CLAMP_TO_EDGE))
    )
)

AYU_DESCRIBE(glow::TextureMagFilter,
    values(
        value("GL_NEAREST", TextureMagFilter(GL_NEAREST)),
        value("GL_LINEAR", TextureMagFilter(GL_LINEAR))
    )
)

AYU_DESCRIBE(glow::TextureMinFilter,
    values(
        value("GL_NEAREST", TextureMinFilter(GL_NEAREST)),
        value("GL_LINEAR", TextureMinFilter(GL_LINEAR)),
        value("GL_NEAREST_MIPMAP_NEAREST", TextureMinFilter(GL_NEAREST_MIPMAP_NEAREST)),
        value("GL_LINEAR_MIPMAP_NEAREST", TextureMinFilter(GL_LINEAR_MIPMAP_NEAREST)),
        value("GL_NEAREST_MIPMAP_LINEAR", TextureMinFilter(GL_NEAREST_MIPMAP_LINEAR)),
        value("GL_LINEAR_MIPMAP_LINEAR", TextureMinFilter(GL_LINEAR_MIPMAP_LINEAR))
    )
)

AYU_DESCRIBE(glow::Texture,
    attrs(
        attr("target", value_funcs<TextureTarget>(
            [](const Texture& v){ return TextureTarget(v.target); },
            [](Texture& v, TextureTarget m){ v = Texture(m); }
        ), optional),
        attr("wrap", value_funcs<TextureWrap>(
            [](const Texture& v){
                glBindTexture(v.target, v.id);
                 // Can't return both S and T so just pick one
                i32 r = 0;
                glGetTexParameteriv(v.target, GL_TEXTURE_WRAP_S, &r);
                return TextureWrap(r);
            },
            [](Texture& v, TextureWrap m){
                glBindTexture(v.target, v.id);
                glTexParameteri(v.target, GL_TEXTURE_WRAP_S, m);
                glTexParameteri(v.target, GL_TEXTURE_WRAP_T, m);
            }
        ), optional),
        attr("wrap_s", value_funcs<TextureWrap>(
            [](const Texture& v){
                glBindTexture(v.target, v.id);
                i32 r = 0;
                glGetTexParameteriv(v.target, GL_TEXTURE_WRAP_S, &r);
                return TextureWrap(r);
            },
            [](Texture& v, TextureWrap m){
                glBindTexture(v.target, v.id);
                glTexParameteri(v.target, GL_TEXTURE_WRAP_S, m);
            }
        ), optional),
        attr("wrap_t", value_funcs<TextureWrap>(
            [](const Texture& v){
                glBindTexture(v.target, v.id);
                i32 r = 0;
                glGetTexParameteriv(v.target, GL_TEXTURE_WRAP_T, &r);
                return TextureWrap(r);
            },
            [](Texture& v, TextureWrap m){
                glTexParameteri(v.target, GL_TEXTURE_WRAP_T, m);
            }
        ), optional),
        attr("mag_filter", value_funcs<TextureMagFilter>(
            [](const Texture& v){
                glBindTexture(v.target, v.id);
                i32 r = 0;
                glGetTexParameteriv(v.target, GL_TEXTURE_MAG_FILTER, &r);
                return TextureMagFilter(r);
            },
            [](Texture& v, TextureMagFilter m){
                glBindTexture(v.target, v.id);
                glTexParameteri(v.target, GL_TEXTURE_MAG_FILTER, m);
            }
        ), optional),
        attr("min_filter", value_funcs<TextureMinFilter>(
            [](const Texture& v){
                glBindTexture(v.target, v.id);
                i32 r = 0;
                glGetTexParameteriv(v.target, GL_TEXTURE_MAG_FILTER, &r);
                return TextureMinFilter(r);
            },
            [](Texture& v, TextureMinFilter m){
                glBindTexture(v.target, v.id);
                glTexParameteri(v.target, GL_TEXTURE_MAG_FILTER, m);
            }
        ), optional),
        attr("filter", value_funcs<TextureMagFilter>(
            [](const Texture& v){
                glBindTexture(v.target, v.id);
                i32 r = 0;
                glGetTexParameteriv(v.target, GL_TEXTURE_MAG_FILTER, &r);
                return TextureMagFilter(r);
            },
            [](Texture& v, TextureMagFilter m){
                glBindTexture(v.target, v.id);
                glTexParameteri(v.target, GL_TEXTURE_MAG_FILTER, m);
                glTexParameteri(v.target, GL_TEXTURE_MIN_FILTER, m);
            }
        ), optional)
         // We won't bother supporting the more exotic parameters unless we
         //  need them.
    )
)
