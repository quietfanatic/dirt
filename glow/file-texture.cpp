#include "file-texture.h"

#include "gl.h"
#include "load-image.h"

namespace glow {

FileTexture::FileTexture (AnyString filename, u32 target) : Texture(target) {
    if (target) {
        load_texture_from_file(target, filename);
    }
}

FileTexture::~FileTexture () { }

} using namespace glow;

#ifndef TAP_DISABLE_TESTS
#include "../ayu/resources/resource.h"
#include "../tap/tap.h"
#include "colors.h"
#include "test-environment.h"

static tap::TestSet tests ("dirt/glow/file-texture", []{
    using namespace tap;
    using namespace geo;

    TestEnvironment env;

    FileTexture tex (ayu::resource_filename(iri::IRI("test:/image.png")));
    auto size = tex.size();
    is(size, IVec{7, 5}, "Created texture has correct size");

    UniqueArray<RGBA8> got_pixels (area(size));
    glGetTexImage(tex.target, 0, GL_RGBA, GL_UNSIGNED_BYTE, got_pixels.data());
    is(got_pixels[10], RGBA8(0x2674dbff), "Created texture has correct content");
    is(got_pixels[34], RGBA8(0x2674dbff), "Created texture has correct content");

    done_testing();
});

#endif
