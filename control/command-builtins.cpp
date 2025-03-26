#include "command-builtins.h"

#include "../uni/io.h"

namespace control::command {

static void help_ (std::optional<AnyString>) {
    uni::print_utf8("help is NYI, sorry");
}
Command help (help_, "help", "NYI");

static void echo_ (AnyString s) {
    uni::print_utf8(s);
}
Command echo (echo_, "echo", "Print a string to stdout");

static void seq_ (const UniqueArray<Statement>& sts) {
    for (auto& st : sts) st();
}
Command seq (seq_, "seq", "Run multiple commands in a row");

static void toggle_ (const Statement& a, const Statement& b, bool& state) {
    state = !state;
    if (state) a();
    else b();
}
Command toggle (toggle_, "toggle", "Alternate between two commands");

} // namespace control::command
