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

 // This is how you define new commands.  Make static objects of these.
struct CommandBase {
    void(* call )(StatementStorageBase*);
    ayu::Type storage_type;
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
};

template <auto& f>
struct Command : CommandBase {
    Command (u32 m, StaticString n, StaticString d) : CommandBase(
        CommandCaller<f>::get_call(),
        ayu::Type::CppType<typename CommandCaller<f>::Storage>(),
        n.data(), d.data(), n.size(), d.size(),
        m, CommandCaller<f>::max
    ) {
        register_command(this);
    }
};

#define CONTROL_COMMAND(name, min, desc) Command<name> _control_command_##name (min, #name, desc);

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
        if (storage) storage->command->storage_type.delete_((ayu::Mu*)storage);
    }

    constexpr explicit operator bool () const { return storage; }
    void operator() () { storage->command->call(storage); }
};

 // Returns nullptr if not found
const CommandBase* lookup_command (Str name) noexcept;
 // Throws CommandNotFound if not found
const CommandBase* require_command (Str name);

constexpr ayu::ErrorCode e_CommandNameDuplicate = "control::e_CommandNameDuplicate";
constexpr ayu::ErrorCode e_CommandNotFound = "control::e_CommandNotFound";

} // namespace control
