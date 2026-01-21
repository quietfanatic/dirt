#include "command.deprecated.h"

#include "../uni/io.h"

namespace control::command {

static void help (const AnyString&) {
    uni::print_utf8("help is NYI, sorry");
}
CONTROL_COMMAND(help, 0, "NYI");

static void echo (const AnyString& s) {
    uni::print_utf8(s);
}
CONTROL_COMMAND(echo, 1, "Print a string to stdout");

static void seq (UniqueArray<Statement>& sts) {
    for (auto& st : sts) st();
}
CONTROL_COMMAND(seq, 1, "Run multiple commands in a row");

static void toggle (Statement& a, Statement& b, bool& state) {
    state = !state;
    if (state) a();
    else b();
}
CONTROL_COMMAND(toggle, 2, "Alternate between two commands");

} // namespace control::command
