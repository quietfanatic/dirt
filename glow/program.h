#pragma once

#include "common.h"
#include "../ayu/location.h"

namespace glow {

struct Shader {
    explicit Shader (uint type = 0);
    Shader (Shader&& o) : id(o.id) { const_cast<uint&>(o.id) = 0; }
    ~Shader ();

    void compile ();

    const uint id = 0;
    operator uint () const { return id; }
};

struct Program {
    UniqueArray<Shader*> shaders;

    virtual void Program_before_link () { }
    virtual void Program_after_link () { }
    virtual void Program_after_use () { }
    virtual void Program_before_unuse () { }

    Program ();
    Program (Program&& o) : id(o.id) { const_cast<uint&>(o.id) = 0; }
    ~Program ();

    void link ();
    void use ();
    void unuse ();
     // For render debugging
    void validate ();

    const uint id = 0;
    operator uint () const { return id; }
};

struct ShaderCompileFailed : GlowError {
     // TODO: replace with pointer
    ayu::Location location;
     // TODO: replace with UniqueString
    std::string info_log;
    ShaderCompileFailed (ayu::Location l, std::string i) :
        location(move(l)), info_log(move(i))
    { }
};
struct ProgramLinkFailed : GlowError {
    ayu::Location location;
    std::string info_log;
    ProgramLinkFailed (ayu::Location l, std::string i) :
        location(move(l)), info_log(move(i))
    { }
};

} // namespace glow
