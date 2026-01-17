#include "registry.internal.h"

#include "command-base.h"

namespace control::in {

struct UnknownCommand : CommandBase<UnknownCommand> { };

[[gnu::cold, noreturn]] NOINLINE
void raise_CommandNameDuplicate (const void* command) {
    auto cmd = (const UnknownCommand*)command;
    raise(e_CommandNameDuplicate, cat(
        "Duplicate command name: ", cmd->name
    ));
}

[[gnu::cold, noreturn]] NOINLINE
void raise_CommandNotFound (Str name) {
    raise(e_CommandNotFound, cat("No command named: ", name));
}

NOINLINE
void register_command (const void* command, void* registry) {
    auto cmd = (const UnknownCommand*)command;
    auto reg = (UniqueArray<const UnknownCommand*>*)registry;
    for (auto c : *reg) {
        if (c->name_hash == cmd->name_hash && c->name == cmd->name) {
            raise_CommandNameDuplicate(cmd);
        }
    }
    reg->emplace_back(cmd);
}

 // TODO: binary search
NOINLINE
const void* lookup_command (Str name, const void* registry) noexcept {
    auto reg = (const UniqueArray<const UnknownCommand*>*)registry;
    auto h = uni::hash(name);
    for (auto c : *reg) {
        if (h == c->name_hash && name == c->name) {
            return c;
        }
    }
    return null;
}

const void* get_command (Str name, const void* registry) {
    if (auto r = lookup_command(name, registry)) return r;
    else raise_CommandNotFound(name);
}

} // control

#ifndef TAP_DISABLE_TESTS
#include "statement.h"
#include "../ayu/reflection/describe-base.h"
#include "../ayu/traversal/from-tree.h"

namespace control::test {

struct TestCommand : CommandBase<TestCommand, void, int&> {
    using CommandBase<TestCommand, void, int&>::CommandBase;
};
using TestStatement = Statement<TestCommand>;

void test (int& out, int in) {
    out = in;
}
CONTROL_COMMAND_FUNCTION(TestCommand, test, 1)

} // control::test

AYU_DESCRIBE(control::test::TestCommand)

static tap::TestSet tests ("dirt/control/registry", []{
    using namespace tap;
    using namespace control::test;
    int result = 0;
    TestStatement st;
    ayu::item_from_string(&st, "[test 444]");
    st(result);
    is(result, 444, "Test command worked.");
    done_testing();
});

#endif
