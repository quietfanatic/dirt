#include "command.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/from-tree.h"
#include "../ayu/traversal/to-tree.h"

namespace control {

static std::unordered_map<Str, const Command*>& commands_by_name () {
    static std::unordered_map<Str, const Command*> r;
    return r;
}

void register_command (const Command* cmd) {
    auto [iter, emplaced] = commands_by_name().emplace(cmd->name(), cmd);
    if (!emplaced) ayu::raise(e_CommandNameDuplicate, cat(
        "Duplicate command name ", cmd->name()
    ));
}

const Command* lookup_command (Str name) noexcept {
    auto& by_name = commands_by_name();
    auto iter = by_name.find(name);
    if (iter != by_name.end()) return iter->second;
    else return nullptr;
}
const Command* require_command (Str name) {
    if (auto r = lookup_command(name)) return r;
    else ayu::raise(e_CommandNotFound, name);
}

static constexpr Statement empty_Statement;

} using namespace control;

AYU_DESCRIBE(control::StatementStorageBase,
    delegate(mixed_funcs<AnyString>(
        [](const StatementStorageBase& v)->AnyString{
            return v.command->name();
        },
        [](StatementStorageBase& v, const AnyString& m){
            v.command = require_command(m);
        }
    ))
)

AYU_DESCRIBE(control::Statement,
    values_custom(
        [](const Statement& a, const Statement&){
            return !a.storage;
        },
        [](Statement& a, const Statement&){
            a = {};
        },
        value_ptr(ayu::Tree::array(), &empty_Statement)
    ),
    before_from_tree([](Statement& v, const ayu::Tree& t){
        auto a = Slice<ayu::Tree>(t);
         // Empty array should be caught by value above
        auto name = Str(a[0]);
        auto command = require_command(name);
        if (a.size() - 1 < command->min || a.size() - 1 > command->max) {
            raise(ayu::e_LengthRejected, cat(
                "Wrong number of arguments to command ", name,
                " (expected ", command->min, "..", command->max,
                " but got ", a.size() - 1, ')'
            ));
        }
        v.storage = (StatementStorageBase*)command->storage_type.default_new();
        v.storage->command = command;
    }),
    delegate(anyptr_func([](Statement& v)->ayu::AnyPtr{
        return ayu::AnyPtr(
            v.storage->command->storage_type,
            (ayu::Mu*)v.storage
        );
    }))
)

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

static UniqueArray<int> test_vals;
static void _test_command (int a, int b) {
    test_vals.push_back(a * b);
}
CONTROL_COMMAND(_test_command, 1, "Command for testing, do not use.");

static tap::TestSet tests ("dirt/control/command", []{
    using namespace tap;

    skip(3);
    //Statement s (&test_command, 3, 4);
    //doesnt_throw([&]{
    //    s();
    //}, "Can create a command in C++");
    //is(test_vals.size(), usize(1), "Can call command");
    //is(test_vals.back(), 12, "Command gave correct result");

    Statement s = Statement();

    doesnt_throw([&]{
        ayu::item_from_string(&s, "[_test_command 5 6]");
    }, "Can create command from ayu");
    doesnt_throw([&]{
        s();
    }, "Can call command");
    is(test_vals.back(), 30, "Command gave correct result");

    is(ayu::item_to_string(&s), "[_test_command 5 6]",
        "Command serializes correctly"
    );

    throws<ayu::Error>([&]{
        ayu::item_from_string(&s, "[_test_command]");
    }, "Can't create command with too few args");

    throws<ayu::Error>([&]{
        ayu::item_from_string(&s, "[_test_command 1 2 3]");
    }, "Can't create command with too many args");

    test_vals = {};
    doesnt_throw([&]{
        ayu::item_from_string(&s, "[seq [[_test_command 5 6] [_test_command 7 8]]]");
        s();
    }, "seq command");
    is(test_vals.size(), usize(2), "seq command works");

    done_testing();
});
#endif
