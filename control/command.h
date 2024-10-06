// A function type that can be used with ayu, to make a non-turing-complete
// imperative DSL.

#pragma once

#include <utility>
#include "../ayu/reflection/anyval.h" 
 // For tuple describability
#include "../ayu/reflection/describe-standard.h"
#include "../ayu/reflection/type.h"
#include "../uni/common.h"
#include "command-template-utils.h"

namespace control {
using namespace uni;

template <class F>
using Function = F;

 // This is how you define new commands.  Make static objects of these.
struct Command {
    Function<void(void*, void*)>* wrapper;
    void* function;
    StaticString name;
    StaticString description;
    u32 required_arg_count;
    Function<ayu::Type()>* args_type;

    template <class... Args>
    constexpr Command (
        Function<void(Args...)> f,
        StaticString name, StaticString desc = "", u32 req = sizeof...(Args)
    ) :
        wrapper(
            CommandWrapper<Args...>::get_unwrap(std::index_sequence_for<Args...>{})
        ),
        function(*reinterpret_cast<void**>(&f)),
        name(name), description(desc), required_arg_count(req),
        args_type([]{
            return ayu::Type::CppType<StatementStorage<Args...>>();
        })
    {
         // Make sure we aren't on a really weird architecture.
        static_assert(sizeof(decltype(f)) == sizeof(void*));
        register_command();
    }
  private:
    void register_command () const;
};

 // Returns nullptr if not found
const Command* lookup_command (Str name) noexcept;
 // Throws CommandNotFound if not found
const Command* require_command (Str name);

 // The structure you create to use a command.  You can create this manually,
 // but it doesn't support optional arguments unless you deserialize from ayu.
struct Statement {
    const Command* command = null;
    ayu::AnyVal args;  // Type must be command->args_type (std::tuple)

    constexpr Statement() { }
    Statement (Command* c, ayu::AnyVal&& a);
    template <class... Args>
    Statement (Command* c, Args... args) :
        Statement(c, ayu::AnyVal::make<StatementStorage<Args...>>(
            std::forward<Args>(args)...
        ))
    { }
    template <class... Args>
    Statement (Str name, Args... args) :
        Statement(require_command(name), std::forward<Args>(args)...)
    { }

     // Run the command
    void operator() () const;

    explicit operator bool () const { return !!command; }
};

constexpr ayu::ErrorCode e_CommandNameDuplicate = "control::e_CommandNameDuplicate";
constexpr ayu::ErrorCode e_CommandNotFound = "control::e_CommandNotFound";
constexpr ayu::ErrorCode e_StatementArgsTypeIncorrect = "control::e_StatementArgsTypeIncorrect";

} // namespace control
