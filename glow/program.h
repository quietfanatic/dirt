#pragma once

#include "common.h"

namespace glow {

struct Shader {
    explicit Shader (u32 type = 0);
    Shader (Shader&& o) : id(o.id) { const_cast<u32&>(o.id) = 0; }
    ~Shader ();

    void compile ();

    const u32 id = 0;
    operator u32 () const { return id; }
};

struct Program {
    UniqueArray<Shader*> shaders;

    virtual void Program_before_link () { }
    virtual void Program_after_link () { }
    virtual void Program_after_use () { }
    virtual void Program_before_unuse () { }

    Program ();
    Program (Program&& o) : id(o.id) { const_cast<u32&>(o.id) = 0; }
    ~Program ();

    void link ();
    void use ();
    void unuse ();
     // For render debugging
    void validate ();

    const u32 id = 0;
    operator u32 () const { return id; }
};

constexpr uni::ErrorCode e_ShaderCompileFailed = "glow::e_ShaderCompileFailed";
constexpr uni::ErrorCode e_ProgramLinkFailed = "glow::e_ProgramLinkFailed";

} // namespace glow
