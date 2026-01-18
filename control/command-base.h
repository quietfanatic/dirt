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

template <class F> using Function = F;

template <class Cmd, class Ret = void, class Ctx = Nothing>
struct CommandBase {

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
            ayu::Type::For_constexpr<typename Convert::type>(),
            n, std::forward<Extra>(extra)...
        );
    }

    template <auto f, class... Extra>
    static consteval Cmd collapsed (
        StaticString n, Extra&&... extra
    ) {
        using Convert = ConvertToCollapsedHandler<Cmd, f>;
        return Cmd(
            Convert::get_handler(),
            ayu::Type::For_constexpr<typename Convert::type>(),
            n, std::forward<Extra>(extra)...
        );
    }

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

template <class Cmd, class Ret, class Nothing>
constinit UniqueArray<const Cmd*> CommandBase<Cmd, Ret, Nothing>::registry;

 // Tried to register multiple commands with the same name in the same domain
constexpr uni::ErrorCode e_CommandNameDuplicate = "control::e_CommandNameDuplicate";
 // Tried to get a command that doesn't exist in this domain
constexpr uni::ErrorCode e_CommandNotFound = "control::e_CommandNotFound";

} // control

///// MACROS

#ifdef __GNUC__
#define CONTROL_REGISTER_COMMAND(cmd) \
[[gnu::constructor]] inline void _control_init_##cmd () { cmd.init(); }
#else
#define CONTROL_REGISTER_COMMAND(cmd) \
[[maybe_unused]] inline const bool _control_init_##cmd = (cmd.init(), false);
#endif

#define CONTROL_COMMAND_FUNCTION(Cmd, f, min, ...) \
constexpr Cmd _control_command_##f = \
    Cmd::function<&f, min>(#f __VA_OPT__(,) __VA_ARGS__); \
CONTROL_REGISTER_COMMAND(_control_command_##f)

#define CONTROL_COMMAND_COLLAPSED(Cmd, f, ...) \
constexpr Cmd _control_command_##f = \
    Cmd::collapsed<&f>(#f __VA_OPT__(,) __VA_ARGS__); \
CONTROL_REGISTER_COMMAND(_control_command_##f)

#define CONTROL_COMMAND_METHOD(Cmd, Ctx, m, min, ...) \
constexpr Cmd _control_command_##m = \
    Cmd::function<&Ctx::m, min>(#m __VA_OPT__(,) __VA_ARGS__); \
CONTROL_REGISTER_COMMAND(_control_command_##m)

#define CONTROL_COMMAND_METHOD_COLLAPSED(Cmd, Ctx, m, ...) \
constexpr Cmd _control_command_##m = \
    Cmd::collapsed<&Ctx::m>(#m __VA_OPT__(,) __VA_ARGS__); \
CONTROL_REGISTER_COMMAND(_control_command_##m)

