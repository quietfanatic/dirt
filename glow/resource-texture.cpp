#include "resource-texture.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/resources/resource.h"
#include "../uni/io.h"
#include "gl.h"
#include "load-image.h"

#ifdef GLOW_PROFILING
#include "../uni/time.h"
#endif

namespace glow {

ResourceTexture::ResourceTexture (u32 target) : Texture(target) {
    if (target) {
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
}

void ResourceTexture::load () {
    glBindTexture(target, id);
    load_texture_from_file(target, ayu::resource_filename(source));
}

ResourceTexture::~ResourceTexture () { }

} using namespace glow;

AYU_DESCRIBE(glow::ResourceTexture,
    attrs(
        attr("glow::Texture", base<Texture>(), include),
        attr("source", &ResourceTexture::source)
    ),
    init<&ResourceTexture::load>()
)

#ifndef TAP_DISABLE_TESTS
#include "../ayu/resources/resource.h"
#include "../tap/tap.h"
#include "colors.h"
#include "test-environment.h"

static tap::TestSet tests ("dirt/glow/resource-texture", []{
    using namespace tap;
    using namespace geo;

    TestEnvironment env;

    ResourceTexture tex;
    tex.source = iri::IRI("test:/image.png");
    tex.load();
    auto size = tex.size();
    is(size, IVec{7, 5}, "Created texture has correct size");

    UniqueArray<RGBA8> got_pixels (area(size));
    glGetTexImage(tex.target, 0, GL_RGBA, GL_UNSIGNED_BYTE, got_pixels.data());
    is(got_pixels[10], RGBA8(0x2674dbff), "Created texture has correct content");

    done_testing();
});

#endif
