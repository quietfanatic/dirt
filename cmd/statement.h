// A function type that can be used with ayu, to make a non-turing-complete
// imperative DSL.

#pragma once

#include "command-base.h"
#include "../ayu/reflection/describe-standard.h"
#include "../ayu/reflection/type.h"

namespace cmd {
using namespace uni;

///// STATEMENT

template <class Cmd>
struct Statement {
    const Cmd* command;
    void* args;

    constexpr Statement () : command(null), args(null) { }
    constexpr Statement (decltype(command) c, void*&& a) :
        command(c), args(a)
    { }
    constexpr Statement (Statement&& o) :
        command(o.command), args(o.args) {
        o.command = null; o.args = null;
    }
    constexpr Statement& operator= (Statement&& o) {
        this->~Statement();
        command = o.command; args = o.args;
        o.command = null; o.args = null;
        return *this;
    }
    constexpr ~Statement () {
        if (args) {
            ayu::dynamic_delete(command->args_type, (ayu::Mu*)args);
        }
    }

    constexpr explicit operator bool () const { return args; }

    void operator() (Cmd::Context ctx) {
        command->handler(ctx, args);
    }
};

} // cmd

AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class Cmd),
    AYU_DESCRIBE_TEMPLATE_TYPE(cmd::Statement<Cmd>),
    desc::computed_name([]()->uni::SharedString{
        return ayu::in::make_template_name_1(
            "cmd::Statement<", ayu::Type::of<Cmd>()
        );
    }),
    desc::to_tree([](const cmd::Statement<Cmd>& v){
        if (!v.args) return ayu::Tree::array();
        else return ayu::Tree();
    }),
    desc::from_tree([](cmd::Statement<Cmd>& v, const ayu::Tree& t){
        v = {};
        return !uni::Slice<ayu::Tree>(t);
    }),
    desc::elems(
        desc::elem(desc::template funcs(
            [](const cmd::Statement<Cmd>& v)->uni::SharedString{
                if (!v) return "";
                return v.command->name;
            },
            [](cmd::Statement<Cmd>& v, uni::Str m){
                v = {};
                v.command = Cmd::get(m);
                v.args = ayu::dynamic_default_new(v.command->args_type);
            }
        )),
        desc::elem(desc::anyptr_func(
            [](cmd::Statement<Cmd>& v){
                if (!v) return ayu::AnyPtr();
                return ayu::AnyPtr(v.command->args_type, (ayu::Mu*)v.args);
            }
        ), desc::include)
    )
)

