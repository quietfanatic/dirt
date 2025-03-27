#include "command.h"

#include "../uni/io.h"

namespace control::command {

static void help_ (std::optional<AnyString>) {
    uni::print_utf8("help is NYI, sorry");
}
Command<help_> help (0, "help", "NYI");

static void echo_ (AnyString s) {
    uni::print_utf8(s);
}
Command<echo_> echo (1, "echo", "Print a string to stdout");

static void seq_ (UniqueArray<Statement>& sts) {
    for (auto& st : sts) st();
}
Command<seq_> seq (1, "seq", "Run multiple commands in a row");

static void toggle_ (Statement& a, Statement& b, bool& state) {
    state = !state;
    if (state) a();
    else b();
}
Command<toggle_> toggle (2, "toggle", "Alternate between two commands");

} // namespace control::command
