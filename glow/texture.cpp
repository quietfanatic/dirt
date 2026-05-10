#include "texture.h"

#include "../ayu/reflection/describe.h"
#include "gl.h"
#include "image.h"

namespace glow {

///// IMPLEMENTATIONS

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

IVec Texture::size (i32 level) {
    IVec r;
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

void texture_from_file (u32 target, SharedString filepath) {
#ifdef GLOW_USE_SAIL
    texture_from_file_sail(target, move(filepath));
#else
    texture_from_file_qoi(target, move(filepath));
#endif
}

void texture_from_image (u32 target, const ImageView& img) noexcept {
    require(img.size.x > 0 && img.size.y > 0);
    if (img.contiguous()) {
        glTexImage2D(
            target, 0, GL_RGBA8,
            img.size.x, img.size.y, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, img.pixels
        );
    }
    else {
        UniqueImage contiguated (img.size);
        blit(contiguated, img);
        glTexImage2D(
            target, 0, GL_RGBA8,
            contiguated.size.x, contiguated.size.y, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, contiguated.pixels
        );
    }
}

 // texture_from_file_sail is in image-sail.cpp
void texture_from_file_qoi (u32 target, SharedString filepath) {
     // TODO: detect 3-channel file and use GL_RGB8
    UniqueImage image = image_from_file_qoi(move(filepath));
    texture_from_image(target, image);
}

ImageTexture::ImageTexture () : Texture(GL_TEXTURE_2D) {
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

PixelTexture::PixelTexture () {
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

void ImageTexture::init () {
    if (target && source) {
        glBindTexture(target, id);
        texture_from_image(target, source_view());
    }
}

///// SERIALIZATION

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
        attr("target", funcs(
            [](const Texture& v){ return TextureTarget(v.target); },
            [](Texture& v, TextureTarget m){ v = Texture(m); }
        ), optional),
        attr("wrap", funcs(
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
        ), optional|invisible),
        attr_default("wrap_s", funcs(
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
        ), "GL_REPEAT", optional),
        attr_default("wrap_t", funcs(
            [](const Texture& v){
                glBindTexture(v.target, v.id);
                i32 r = 0;
                glGetTexParameteriv(v.target, GL_TEXTURE_WRAP_T, &r);
                return TextureWrap(r);
            },
            [](Texture& v, TextureWrap m){
                glBindTexture(v.target, v.id);
                glTexParameteri(v.target, GL_TEXTURE_WRAP_T, m);
            }
        ), "GL_REPEAT", optional),
        attr_default("mag_filter", funcs(
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
        ), "GL_LINEAR", optional),
        attr_default("min_filter", funcs(
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
        ), "GL_NEAREST_MIPMAP_LINEAR", optional),
        attr("filter", funcs(
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
        ), optional|invisible)
         // We won't bother supporting the more exotic parameters unless we
         //  need them.
    )
)

AYU_DESCRIBE(glow::ImageTexture,
    attrs(
         // TODO: figure out how to make this optional without regenning texture
        attr("Texture", base<Texture>(), include),
        attr("source", &ImageTexture::source),
        attr("bounds", &ImageTexture::bounds, optional)
    ),
    init([](ImageTexture& v){ v.init(); })
)

AYU_DESCRIBE(glow::PixelTexture,
    delegate(base<ImageTexture>())
)

///// TESTS

#ifndef TAP_DISABLE_TESTS
#include "../ayu/traversal/to-tree.h"
#include "../tap/tap.h"
#include "../wind/window.h"
#include "program.h"
#include "test/test-environment.h"

namespace glow::test {

struct TextureProgram : Program {
    i32 u_screen_rect = -1;
    i32 u_tex_rect = -1;

    void Program_after_link () override {
        u_screen_rect = glGetUniformLocation(id, "u_screen_rect");
        u_tex_rect = glGetUniformLocation(id, "u_tex_rect");
        i32 u_tex = glGetUniformLocation(id, "u_tex");
        glUniform1i(u_tex, 0);
        require(u_screen_rect != -1);
        require(u_tex_rect != -1);
        require(u_tex != -1);
    }
};

void draw_texture (
    const Texture& tex, const Rect& screen_rect, const Rect& tex_rect = {0,0,1,1}
) {
    require(!!tex);
    require(tex.target == GL_TEXTURE_2D);

    static TextureProgram* program = ayu::track(
        program, "test:/texture-test.ayu#program"
    );

    glUniform1fv(program->u_screen_rect, 4, &screen_rect.l);
    glUniform1fv(program->u_tex_rect, 4, &tex_rect.l);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

} using namespace glow::test;

AYU_DESCRIBE(glow::test::TextureProgram,
    delegate(base<Program>())
)

static tap::TestSet tests ("dirt/glow/texture", []{
    using namespace tap;

    TestEnvironment env;

    ImageTexture* tex;
    doesnt_throw([&]{
        tex = ayu::link_from_iri("test:/texture-test.ayu#texture");
    }, "Can load texture");

    ImageTexture* tex2;
    doesnt_throw([&]{
        tex2 = ayu::link_from_iri("test:/texture-test.ayu#texture2");
    }, "Can load texture from file image");

    auto fi = static_cast<FileImage*>(tex2->source);
    ok(!!fi->pixels, "FileImage was not automatically trimmed");
    fi->trim();
    ok(!fi->pixels, "Can trim FileImage");

    RGBA8 bg = u32(0x331100ee);
    RGBA8 fg = u32(0x2674dbf0);
    RGBA8 fg2 = u32(0x2674dbff);

    is(tex->size(), IVec{7, 5}, "Created texture has correct size");
    is(tex2->size(), IVec{7, 5}, "File image texture has correct size");

    UniqueImage tex_image (tex->size());
    glBindTexture(tex->target, *tex);
    glGetTexImage(tex->target, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_image.pixels);
    is(tex_image[{4, 3}], fg, "Created texture has correct content");

    glBindTexture(tex2->target, *tex2);
    UniqueImage tex2_image (tex2->size());
    glGetTexImage(tex2->target, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex2_image.pixels);
    is(tex2_image[{4, 3}], fg2, "File image texture has corrent content");

    glClearColor(bg.r/255.f, bg.g/255.f, bg.b/255.f, bg.a/255.f);
    glClear(GL_COLOR_BUFFER_BIT);

    doesnt_throw([&]{
        draw_texture(*tex2, Rect{-.5, -.5, .5, .5});
    }, "Can draw texture");

    UniqueImage expected (env.size);
    for (i32 y = 0; y < env.size.y; y++)
    for (i32 x = 0; x < env.size.x; x++) {
        if (y >= env.size.y / 4 && y < env.size.y * 3 / 4
         && x >= env.size.x / 4 && x < env.size.x * 3 / 4) {
            expected[{x, y}] = fg2;
        }
        else {
            expected[{x, y}] = bg;
        }
    }

    UniqueImage got = env.read_pixels();

    bool match = true;
    for (i32 y = 0; y < env.size.y; y++)
    for (i32 x = 0; x < env.size.x; x++) {
        if (expected[{x, y}] != got[{x, y}]) {
            match = false;
            diag(ayu::show(&expected[{x, y}]));
            diag(ayu::show(&got[{x, y}]));
            goto no_match;
        }
    }
    no_match:;
    if (!ok(match, "Texture program wrote correct pixels")) {
         // NOTE: these images will be upside-down.
         // TODO: bring image parsing/saving back
//        expected.save(ayu::resource_filename("/dirt/glow/test/texture-fail-expected"));
//        got.save(ayu::resource_filename("/dirt/glow/test/texture-fail-got"));
    }
//    SDL_GL_SwapWindow(window.sdl_window);
//    SDL_Delay(5000);

    done_testing();
});
#endif
