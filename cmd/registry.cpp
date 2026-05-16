#include "registry.internal.h"

#include "../uni/io.h"
#include "command-base.h"

namespace cmd::in {

struct UnknownCommand : CommandBase<UnknownCommand> { };

[[gnu::cold, noreturn]] NOINLINE
void raise_CommandNotFound (Str name) {
    raise(e_CommandNotFound, cat("No command named: ", name));
}

NOINLINE
void register_command (void* registry, const void* command) noexcept {
    auto reg = (UnknownCommand::Registry*)registry;
    auto cmd = (const UnknownCommand*)command;
    auto hash = uni::hash(cmd->name);
    for (auto [h, c] : *reg) {
        if (hash == h && c->name == cmd->name) {
            warn_utf8(cat("Duplicate command name: ", cmd->name));
            abort();
        }
    }
    reg->emplace_back(hash, cmd);
}

NOINLINE
const void* lookup_command (const void* registry, Str name) noexcept {
    auto reg = (const UnknownCommand::Registry*)registry;
    auto hash = uni::hash(name);
     // We could possibly use a binary search, but it isn't worth the extra
     // complexity.  This isn't particularly performance-critical.
    for (auto [h, c] : *reg) {
        if (hash == h && name == c->name) {
            return c;
        }
    }
    [[unlikely]] return null;
}

const void* get_command (const void* registry, Str name) {
    if (auto r = lookup_command(registry, name)) return r;
    else raise_CommandNotFound(name);
}

} // cmd

#ifndef TAP_DISABLE_TESTS
#include "statement.h"
#include "../ayu/reflection/describe-base.h"
#include "../ayu/traversal/from-tree.h"

namespace cmd::test {

struct TestCommand : CommandBase<TestCommand, void(int&)> {
    using CommandBase<TestCommand, void(int&)>::CommandBase;
};
using TestStatement = Statement<TestCommand>;

void test (int& out, int in) {
    out = in;
}
CMD_COMMAND_FUNCTION(TestCommand, test, 1)

} // cmd::test

AYU_DESCRIBE(cmd::test::TestCommand)

static tap::TestSet tests ("dirt/cmd/registry", []{
    using namespace tap;
    using namespace cmd::test;
    int result = 0;
    TestStatement st;
    ayu::item_from_string(&st, "[test 444]");
    st(result);
    is(result, 444, "Test command worked.");
    done_testing();
});

#endif
