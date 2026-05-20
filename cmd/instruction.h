#pragma once

#include "command-base.h"
#include "../ayu/reflection/describe-standard.h"
#include "../ayu/reflection/type.h"

namespace cmd {
using namespace uni;

template <class Cmd>
struct Instruction {
    const Cmd* command;
    void* args;

    constexpr Instruction () : command(null), args(null) { }
    constexpr Instruction (decltype(command) c, void*&& a) :
        command(c), args(a)
    { }
    constexpr Instruction (Instruction&& o) :
        command(o.command), args(o.args) {
        o.command = null; o.args = null;
    }
    constexpr Instruction& operator= (Instruction&& o) {
        this->~Instruction();
        command = o.command; args = o.args;
        o.command = null; o.args = null;
        return *this;
    }
    constexpr ~Instruction () {
        if (args) {
            ayu::dynamic_delete(command->args_type, (ayu::Mu*)args);
        }
    }

     // Check command instead of args, because as a future optimization we might
     // leave args null for commands with no arguments.
    constexpr explicit operator bool () const { return command; }

    auto operator() (Cmd::Context ctx) {
        return command->handler(ctx, args);
    }
};

} // cmd

AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class Cmd),
    AYU_DESCRIBE_TEMPLATE_TYPE(cmd::Instruction<Cmd>),
    desc::computed_name([]()->uni::SharedString{
        return ayu::in::make_template_name_1(
            "cmd::Instruction<", ayu::Type::of<Cmd>()
        );
    }),
    desc::to_tree([](const cmd::Instruction<Cmd>& v){
        if (!v.args) return ayu::Tree::array();
        else return ayu::Tree();
    }),
    desc::from_tree([](cmd::Instruction<Cmd>& v, const ayu::Tree& t){
        v = {};
        return !uni::Slice<ayu::Tree>(t);
    }),
    desc::elems(
        desc::elem(desc::template funcs(
            [](const cmd::Instruction<Cmd>& v)->uni::SharedString{
                if (!v) return "";
                return v.command->name;
            },
            [](cmd::Instruction<Cmd>& v, uni::Str m){
                v = {};
                v.command = Cmd::get(m);
                v.args = ayu::dynamic_default_new(v.command->args_type);
            }
        )),
        desc::elem(desc::anyptr_func(
            [](cmd::Instruction<Cmd>& v){
                if (!v) return ayu::AnyPtr();
                return ayu::AnyPtr(v.command->args_type, (ayu::Mu*)v.args);
            }
        ), desc::include)
    )
)

