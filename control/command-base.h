// A function type that can be used with ayu, to make a non-turing-complete
// imperative DSL.

#pragma once

#include "../uni/common.h"
#include "../uni/hash.h"
#include "args-tuple.h"
#include "registry.internal.h"

namespace control {
using namespace uni;

///// COMMAND

template <class Cmd, class Sig = void(Nothing)>
struct CommandBase;

template <class Cmd, class Ret, class Ctx>
struct CommandBase<Cmd, Ret(Ctx)> {

    using Return = Ret;
    using Context = Ctx;

    using Handler = Return (Context, void*);

    Handler* handler;
    ayu::Type args_type;
    StaticString name;
    usize name_hash;

    constexpr CommandBase (
        Handler* h,
        ayu::Type a,
        StaticString n
    ) :
        handler(h),
        args_type(a),
        name(n),
        name_hash(uni::hash(n))
    { }

     // Named constructor because there's no way to provide explicit template
     // arguments for a normal constructor.
    template <auto f, u32 min, class... Extra>
    static consteval Cmd function (
        StaticString n, Extra&&... extra
    ) {
        using Convert = ConvertToArgsTupleHandler<Cmd, f, min>;
        return Cmd(
            Convert::get_handler(),
            ayu::Type::constexpr_of<typename Convert::type>(),
            n, std::forward<Extra>(extra)...
        );
    }

    template <auto f, class... Extra>
    static consteval Cmd collapse (
        StaticString n, Extra&&... extra
    ) {
        using Convert = ConvertToCollapseHandler<Cmd, f>;
        return Cmd(
            Convert::get_handler(),
            ayu::Type::constexpr_of<typename Convert::type>(),
            n, std::forward<Extra>(extra)...
        );
    }

     // TODO: put hashes in the registry for better cache locality?
    static UniqueArray<const Cmd*> registry;

    NOINLINE void init () const {
        in::register_command(this, &registry);
    }

    static const Cmd* lookup (Str name) noexcept {
        return (const Cmd*)in::lookup_command(name, &registry);
    }

    static const Cmd* get (Str name) {
        return (const Cmd*)in::get_command(name, &registry);
    }
};

template <class Cmd, class Ret, class Ctx>
constinit UniqueArray<const Cmd*> CommandBase<Cmd, Ret(Ctx)>::registry;

 // Tried to register multiple commands with the same name in the same domain
constexpr uni::ErrorCode e_CommandNameDuplicate = "control::e_CommandNameDuplicate";
 // Tried to get a command that doesn't exist in this domain
constexpr uni::ErrorCode e_CommandNotFound = "control::e_CommandNotFound";

} // control

///// MACROS

#ifdef __GNUC__
#define CONTROL_REGISTER_COMMAND(cmd) \
[[gnu::constructor]] static inline void _control_init_##cmd () { cmd.init(); }
#else
#define CONTROL_REGISTER_COMMAND(cmd) \
[[maybe_unused]] static inline const bool _control_init_##cmd = (cmd.init(), false);
#endif

#define CONTROL_COMMAND_FUNCTION_NAME(Cmd, name, f, min, ...) \
constexpr Cmd _control_command_##f = \
    Cmd::function<&f, min>(name __VA_OPT__(,) __VA_ARGS__); \
CONTROL_REGISTER_COMMAND(_control_command_##f)

#define CONTROL_COMMAND_FUNCTION(Cmd, f, min, ...) \
    CONTROL_COMMAND_FUNCTION_NAME(Cmd, #f, f, min, __VA_ARGS__)

#define CONTROL_COMMAND_COLLAPSE_NAME(Cmd, name, f, ...) \
constexpr Cmd _control_command_##f = \
    Cmd::collapse<&f>(name __VA_OPT__(,) __VA_ARGS__); \
CONTROL_REGISTER_COMMAND(_control_command_##f)

#define CONTROL_COMMAND_COLLAPSE(Cmd, f, ...) \
    CONTROL_COMMAND_COLLAPSE_NAME(Cmd, #f, f, __VA_ARGS__)

#define CONTROL_COMMAND_METHOD_NAME(Cmd, name, Ctx, m, min, ...) \
constexpr Cmd _control_command_##m = \
    Cmd::function<&Ctx::m, min>(name __VA_OPT__(,) __VA_ARGS__); \
CONTROL_REGISTER_COMMAND(_control_command_##m)

#define CONTROL_COMMAND_METHOD(Cmd, Ctx, m, min, ...) \
    CONTROL_COMMAND_METHOD_NAME(Cmd, #m, Ctx, m, min, ...)

#define CONTROL_COMMAND_METHOD_COLLAPSE_NAME(Cmd, name, Ctx, m, ...) \
constexpr Cmd _control_command_##m = \
    Cmd::collapse<&Ctx::m>(name __VA_OPT__(,) __VA_ARGS__); \
CONTROL_REGISTER_COMMAND(_control_command_##m)

#define CONTROL_COMMAND_METHOD_COLLAPSE(Cmd, Ctx, m, ...) \
    CONTROL_COMMAND_METHOD_COLLAPSE_NAME(Cmd, #m, Ctx, m, __VA_ARGS__)

