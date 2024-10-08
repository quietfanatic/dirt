#include "command.h"

#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/from-tree.h"
#include "../ayu/traversal/to-tree.h"

namespace control {

static std::unordered_map<Str, const Command*>& commands_by_name () {
    static std::unordered_map<Str, const Command*> r;
    return r;
}

void Command::register_command () const {
    auto [iter, emplaced] = commands_by_name().emplace(name, this);
    if (!emplaced) ayu::raise(e_CommandNameDuplicate, cat(
        "Duplicate command name ", name
    ));
}

const Command* lookup_command (Str name) noexcept {
    auto iter = commands_by_name().find(name);
    if (iter != commands_by_name().end()) return iter->second;
    else return nullptr;
}
const Command* require_command (Str name) {
    if (auto r = lookup_command(name)) return r;
    else ayu::raise(e_CommandNotFound, name);
}

Statement::Statement (Command* c, ayu::AnyVal&& a) : command(c), args(move(a)) {
    if (args.type != command->args_type()) {
        ayu::raise(e_StatementArgsTypeIncorrect, cat(
            "Statement args type for ", command->name, " is incorrect; expected ",
            command->args_type().name(), " but got ", args.type.name()
        ));
    }
}

 // Should this be inlined?
void Statement::operator() () const {
#ifndef DEBUG
    require(args.type == command->args_type());
#endif
    command->wrapper(command->function, args.data);
}

} using namespace control;

AYU_DESCRIBE(const Command*,
    delegate(mixed_funcs<AnyString>(
        [](const Command* const& c)->AnyString{
            return c->name;
        },
        [](const Command*& c, const AnyString& s){
            c = require_command(s);
        }
    ))
)

 // External for debugging.  There are some problems around serializing and
 // deserializing commands containing pointers, revolving around the fact that
 // we can't set the serialization location when we're delegating to
 // item_*_tree on std::tuple and then changing the tree.  Currently everything
 // seems to work, but if you perturb this, it may stop working in mysterious
 // ways.
ayu::Tree Statement_to_tree (const Statement& s) {
     // Serialize the args and stick the command name in front
     // TODO: allow constructing readonly AnyRef from const AnyVal
    auto args_tree = ayu::item_to_tree(
        const_cast<ayu::AnyVal&>(s.args).ptr()
    );
    auto a = AnyArray<ayu::Tree>(move(args_tree));
    a.emplace(a.begin(), Str(s.command->name));
    return ayu::Tree(move(a));
}
void Statement_from_tree (Statement& s, const ayu::Tree& t) {
     // Get the command from the first elem, then args from the rest.
     // TODO: optional parameters
    auto a = AnyArray<ayu::Tree>(t);
    if (a.size() == 0) {
        s = {}; return;
    }
    s.command = require_command(Str(a[0]));
    a.erase(usize(0));
    s.args = ayu::AnyVal(s.command->args_type());
    ayu::item_from_tree(
        s.args.ptr(), ayu::Tree(move(a)), {},
        ayu::FromTreeOptions::DelaySwizzle
    );
}

AYU_DESCRIBE(Statement,
    to_tree(&Statement_to_tree),
    from_tree(&Statement_from_tree)
)

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

static UniqueArray<int> test_vals;
static void test_command_ (int a, int b) {
    test_vals.push_back(a * b);
}
static Command test_command (test_command_, "_test_command", "Command for testing, do not use.", 1);

static tap::TestSet tests ("dirt/control/command", []{
    using namespace tap;

    Statement s (&test_command, 3, 4);
    doesnt_throw([&]{
        s();
    }, "Can create a command in C++");
    is(test_vals.size(), usize(1), "Can call command");
    is(test_vals.back(), 12, "Command gave correct result");

    s = Statement();

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
