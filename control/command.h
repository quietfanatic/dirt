// A function type that can be used with ayu, to make a non-turing-complete
// imperative DSL.

#pragma once

#include <utility>
#include "../uni/common.h"
#include "command.internal.h"

namespace control {
using namespace uni;

template <class F>
using Function = F;

///// COMMAND

 // This is how you declare commands.  Make a function called `name` and then call
 // this with `name`, the minimum required parameters, and a one-line description.
 //
 // static void foo (int arg1, const AnyString& arg2) { ... }
 // CONTROL_COMMAND(foo, 1, "Do some fooery")

struct Command {
    void(* call )(StatementStorageBase*);
    const ayu::Type storage_type;
    const char* name_d;
    const char* desc_d;
    u32 name_s;
    u32 desc_s;
    constexpr StaticString name () const {
        return StaticString(name_d, name_s);
    }
    constexpr StaticString description () const {
        return StaticString(desc_d, desc_s);
    }
    u32 min;
    u32 max;
    consteval Command (
        void(* c )(StatementStorageBase*),
        ayu::Type t,
        StaticString n, StaticString d,
        u32 i, u32 a
    ) :
        call(c), storage_type(t),
        name_d(n.data()), desc_d(d.data()),
        name_s(n.size()), desc_s(d.size()),
        min(i), max(a)
    { expect(n.size() <= u32(-1) && d.size() <= u32(-1)); }
};

#ifdef __GNUC__
#define CONTROL_COMMAND(f, min, desc) \
constexpr control::Command _control_command_##f ( \
    control::CommandCaller<f>::get_call(), \
    ayu::Type::For_constexpr< \
        typename control::CommandCaller<f>::Storage \
    >(), \
    #f, desc, min, control::CommandCaller<f>::max \
); \
[[gnu::constructor]] inline void _control_init_##f () { \
    control::register_command(&_control_command_##f); \
}
#else
#define CONTROL_COMMAND(f, min, desc) \
constexpr control::Command _control_command_##f ( \
    control::CommandCaller<f>::get_call(), \
    ayu::Type::For_constexpr< \
        typename control::CommandCaller<f>::Storage \
    >(), \
    #f, desc, min, control::CommandCaller<f>::max \
); \
[[maybe_unused]] inline const bool _control_init_##f = \
    (control::register_command(&_control_command_##f), false);
#endif
///// STATEMENT

 // The structure you create to use a command.  TODO: creation on the C++ side
 // TODO: more documentation
 // Note that Statements generally should not be const, because they're allowed
 // to carry state and modify it.
struct Statement {
    StatementStorageBase* storage;

    constexpr Statement (StatementStorageBase* s = null) : storage(s) { }
    constexpr Statement (Statement&& o) : storage(o.storage) { 
        o.storage = null;
    }
    constexpr Statement& operator= (Statement&& o) {
        this->~Statement();
        storage = o.storage;
        o.storage = null;
        return *this;
    }
    constexpr ~Statement () {
        if (storage) {
            storage->command->storage_type.delete_((ayu::Mu*)storage);
        }
    }

    constexpr explicit operator bool () const { return storage; }
    void operator() () { storage->command->call(storage); }
};

 // Returns nullptr if not found
const Command* lookup_command (Str name) noexcept;
 // Throws CommandNotFound if not found
const Command* require_command (Str name);

constexpr ayu::ErrorCode e_CommandNameDuplicate = "control::e_CommandNameDuplicate";
constexpr ayu::ErrorCode e_CommandNotFound = "control::e_CommandNotFound";

} // namespace control
