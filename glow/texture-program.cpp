#include "texture-program.h"

#include "../iri/iri.h"
#include "../ayu/resources/resource.h"
#include "gl.h"
#include "image.h"
#include "program.h"

namespace glow {

struct TextureProgram : Program {
    int u_screen_rect = -1;
    int u_tex_rect = -1;

    void Program_after_link () override {
        u_screen_rect = glGetUniformLocation(id, "u_screen_rect");
        u_tex_rect = glGetUniformLocation(id, "u_tex_rect");
        int u_tex = glGetUniformLocation(id, "u_tex");
        glUniform1i(u_tex, 0);
        require(u_screen_rect != -1);
        require(u_tex_rect != -1);
        require(u_tex != -1);
    }
};

void draw_texture (const Texture& tex, const Rect& screen_rect, const Rect& tex_rect) {
    require(!!tex);
    require(tex.target == GL_TEXTURE_2D);

    static constexpr iri::IRI program_location (
        "res:/dirt/glow/texture-program.ayu"
    );
    static TextureProgram* program =
        ayu::ResourceRef(program_location)["program"][1];
    program->use();

    glUniform1fv(program->u_screen_rect, 4, &screen_rect.l);
    glUniform1fv(program->u_tex_rect, 4, &tex_rect.l);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

} using namespace glow;

AYU_DESCRIBE(glow::TextureProgram,
    delegate(base<Program>())
)

#ifndef TAP_DISABLE_TESTS
#include "../ayu/traversal/to-tree.h"
#include "../tap/tap.h"
#include "../wind/window.h"
#include "file-image.h"
#include "image-texture.h"
#include "test-environment.h"

static tap::TestSet tests ("dirt/glow/texture-program", []{
    using namespace tap;

    TestEnvironment env;

    ImageTexture* tex;
    doesnt_throw([&]{
        tex = ayu::ResourceRef(IRI("test:/texture-test.ayu"))["texture"][1];
    }, "Can load texture");

    ImageTexture* tex2;
    doesnt_throw([&]{
        tex2 = ayu::ResourceRef(IRI("test:/texture-test.ayu"))["texture2"][1];
    }, "Can load texture from file image");

    is(dynamic_cast<FileImage*>(tex2->source.image)->storage, null, "File texture was trimmed");

    RGBA8 bg = uint32(0x331100ee);
    RGBA8 fg = uint32(0x2674dbf0);
    RGBA8 fg2 = uint32(0x2674dbff);

    is(tex->size(), IVec{7, 5}, "Created texture has correct size");
    is(tex2->size(), IVec{7, 5}, "File image texture has correct size");

    UniqueImage tex_image (ImageRef(tex->source).size);
    glBindTexture(tex->target, *tex);
    glGetTexImage(tex->target, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_image.pixels);
    is(tex_image[{4, 3}], fg, "Created texture has correct content");

    glBindTexture(tex2->target, *tex2);
    UniqueImage tex2_image (ImageRef(tex2->source).size);
    glGetTexImage(tex2->target, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex2_image.pixels);
    is(tex2_image[{4, 3}], fg2, "File image texture has corrent content");

    glClearColor(bg.r/255.f, bg.g/255.f, bg.b/255.f, bg.a/255.f);
    glClear(GL_COLOR_BUFFER_BIT);

    doesnt_throw([&]{
        draw_texture(*tex, Rect{-.5, -.5, .5, .5});
    }, "Can draw texture");

    UniqueImage expected (env.size);
    for (int y = 0; y < env.size.y; y++)
    for (int x = 0; x < env.size.x; x++) {
        if (y >= env.size.y / 4 && y < env.size.y * 3 / 4
         && x >= env.size.x / 4 && x < env.size.x * 3 / 4) {
            expected[{x, y}] = fg;
        }
        else {
            expected[{x, y}] = bg;
        }
    }

    UniqueImage got = env.read_pixels();

    bool match = true;
    for (int y = 0; y < env.size.y; y++)
    for (int x = 0; x < env.size.x; x++) {
        if (expected[{x, y}] != got[{x, y}]) {
            match = false;
            diag(ayu::item_to_string(&expected[{x, y}]));
            diag(ayu::item_to_string(&got[{x, y}]));
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
