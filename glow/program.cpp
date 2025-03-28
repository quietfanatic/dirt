#include "program.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/to-tree.h"
#include "gl.h"

namespace glow {

Shader::Shader (u32 type) : id(0) {
    if (type) {
        init();
        const_cast<u32&>(id) = glCreateShader(type);
    }
}

Shader::~Shader () {
    if (id) glDeleteShader(id);
}

void Shader::compile () {
    require(id);
    glCompileShader(id);
    i32 status = 0; glGetShaderiv(id, GL_COMPILE_STATUS, &status);
    i32 loglen = 0; glGetShaderiv(id, GL_INFO_LOG_LENGTH, &loglen);
    if (!status || loglen > 16) {
        auto info_log = UniqueString(Uninitialized(loglen));
        glGetShaderInfoLog(id, loglen, nullptr, info_log.data());
        auto self = this;
        ayu::raise(e_ShaderCompileFailed, cat(
            "Failed to compile GL shader at ", ayu::item_to_string(&self),
            ":\n", info_log
        ));
    }
}

static Program* current_program = null;

Program::Program () {
    init();
    const_cast<u32&>(id) = glCreateProgram();
}

Program::~Program () {
    if (id) glDeleteProgram(id);
}

void Program::link () {
    require(id);
     // Detach old shaders
    i32 n_to_detach; glGetProgramiv(id, GL_ATTACHED_SHADERS, &n_to_detach);
    if (n_to_detach) {
        u32 to_detach [n_to_detach];
        glGetAttachedShaders(id, n_to_detach, null, to_detach);
        for (auto s : to_detach) {
            glDetachShader(id, s);
        }
    }
     // Attach new shaders
    for (auto* s : shaders) {
        i32 status = 0; glGetShaderiv(s->id, GL_COMPILE_STATUS, &status);
        if (!status) s->compile();
        glAttachShader(id, s->id);
    }
     // Link
    Program_before_link();
    glLinkProgram(id);
    i32 status = 0; glGetProgramiv(id, GL_LINK_STATUS, &status);
    i32 loglen = 0; glGetProgramiv(id, GL_INFO_LOG_LENGTH, &loglen);
    if (!status || loglen > 16) {
        auto info_log = UniqueString(Uninitialized(loglen));
        glGetProgramInfoLog(id, loglen, nullptr, info_log.data());
        auto self = this;
        ayu::raise(e_ProgramLinkFailed, cat(
            "Failed to link GL program at ", ayu::item_to_string(&self),
            ":\n", info_log
        ));
    }
     // Extra
    if (current_program) current_program->unuse();
    glUseProgram(id);
    current_program = this;
    Program_after_link();
}

void Program::use () {
    if (current_program == this) return;
    if (current_program) {
        current_program->unuse();
    }
    glUseProgram(id);
    current_program = this;
    Program_after_use();
}

void Program::unuse () {
    if (current_program != this) return;
    Program_before_unuse();
    glUseProgram(0);
    current_program = null;
}

void Program::validate () {
    glValidateProgram(id);
    i32 status = 0; glGetProgramiv(id, GL_VALIDATE_STATUS, &status);
    i32 loglen = 0; glGetProgramiv(id, GL_INFO_LOG_LENGTH, &loglen);
    auto info_log = UniqueString(Uninitialized(loglen));
    glGetProgramInfoLog(id, loglen, nullptr, info_log.data());
    ayu::dump(status);
    ayu::dump(info_log);
}

enum ShaderType { };

} using namespace glow;

AYU_DESCRIBE(glow::ShaderType,
    values(
        value(0, ShaderType(0)),
        value("GL_COMPUTE_SHADER", ShaderType(GL_COMPUTE_SHADER)),
        value("GL_VERTEX_SHADER", ShaderType(GL_VERTEX_SHADER)),
        value("GL_TESS_CONTROL_SHADER", ShaderType(GL_TESS_CONTROL_SHADER)),
        value("GL_TESS_EVALUATION_SHADER", ShaderType(GL_TESS_EVALUATION_SHADER)),
        value("GL_GEOMETRY_SHADER", ShaderType(GL_GEOMETRY_SHADER)),
        value("GL_FRAGMENT_SHADER", ShaderType(GL_FRAGMENT_SHADER))
    )
)

AYU_DESCRIBE(glow::Shader,
    attrs(
        attr("type", value_funcs<ShaderType>(
            [](const Shader& v){
                if (v.id) {
                    i32 type = 0;
                    glGetShaderiv(v.id, GL_SHADER_TYPE, &type);
                    return ShaderType(type);
                }
                else return ShaderType(0);
            },
            [](Shader& v, ShaderType type){
                if (v.id) glDeleteShader(v.id);
                if (type) {
                    const_cast<u32&>(v.id) = glCreateShader(type);
                }
            }
        )),
        attr("source", value_funcs<AnyString>(
            [](const Shader& v){
                require(v.id);
                i32 len = 0;
                glGetShaderiv(v.id, GL_SHADER_SOURCE_LENGTH, &len);
                if (!len) return AnyString();
                auto r = AnyString(Uninitialized(len));
                glGetShaderSource(v.id, len, null, r.mut_data());
                return r;
            },
            [](Shader& v, AnyString s){
                const char* src_p = s.c_str();
                i32 src_len = s.size();
                glShaderSource(v.id, 1, &src_p, &src_len);
            }
        ))
    )
)

AYU_DESCRIBE(glow::Program,
    attrs(
        attr("shaders", &Program::shaders)
    ),
    init<&Program::link>()
)

#ifndef TAP_DISABLE_TESTS
#include "../iri/iri.h"
#include "../ayu/resources/resource.h"
#include "../ayu/traversal/to-tree.h"
#include "../geo/rect.h"
#include "../geo/vec.h"
#include "../tap/tap.h"
#include "../wind/window.h"
#include "colors.h"
#include "test-environment.h"

static tap::TestSet tests ("dirt/glow/program", []{
    using namespace tap;
    using namespace geo;
    TestEnvironment env;

    Program* program;
    doesnt_throw([&]{
        program = ayu::ResourceRef(iri::IRI("test:/test-program.ayu"))["program"][1];
    }, "Can load program from ayu document");
    program->use();
    i32 u_screen_rect = glGetUniformLocation(*program, "u_screen_rect");
    isnt(u_screen_rect, -1, "Can get a uniform location");
    auto screen_rect = Rect{-0.5, -0.5, 0.5, 0.5};
    doesnt_throw([&]{
        glUniform1fv(u_screen_rect, 4, &screen_rect.l);
    }, "Can set uniform array");
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    doesnt_throw([&]{
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }, "glDrawArrays");

    UniqueArray<RGBA8> expected_pixels (area(env.size));
    for (i32 y = 0; y < env.size.y; y++)
    for (i32 x = 0; x < env.size.x; x++) {
        if (y >= env.size.y / 4 && y < env.size.y * 3 / 4
         && x >= env.size.x / 4 && x < env.size.x * 3 / 4) {
            expected_pixels[y*env.size.x+x] = RGBA8(30, 40, 50, 60);
        }
        else {
            expected_pixels[y*env.size.x+x] = RGBA8(0, 0, 0, 0);
        }
    }

    UniqueArray<RGBA8> got_pixels (area(env.size));
    glFinish();
    glReadPixels(0, 0, env.size.x, env.size.y, GL_RGBA, GL_UNSIGNED_BYTE, got_pixels.data());

    if (!is(got_pixels, expected_pixels, "Rendered correct image")) {
        diag(ayu::item_to_string(&got_pixels, ayu::PrintOptions::Compact));
        diag(ayu::item_to_string(&expected_pixels, ayu::PrintOptions::Compact));
    }

    done_testing();
});

#endif
