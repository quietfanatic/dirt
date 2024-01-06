#include "shell.h"

#include <cstdio> // Using some POSIX APIs
#include "strings.h"

namespace uni {

bool CommandResult::command_not_found () {
    return WEXITSTATUS(ret) == 127;
}

CommandResult shell (const char* cmd) {
    FILE* o = popen(cmd, "r");
    CommandResult r;
     // This can't be the best way to do this, but whatever
    for (int c = std::fgetc(o); c != EOF; c = std::fgetc(o)) {
        r.out.push_back(c);
    }
    r.ret = pclose(o);
    return r;
}

CommandResult run (Slice<Str> args) {
    return shell(cat(
        Caterator(" ", args.size(), [args](usize i){
            return escape_for_shell(args[i]);
        }),
    '\0').data());
}

UniqueString escape_for_shell (Str in) {
    usize len = in.size() + 2;
    for (auto& c : in) if (c == '\'') len += 3;
    auto out = UniqueString(Capacity(len));
    out.push_back_expect_capacity('\'');
    for (auto& c : in) {
        if (c == '\'') out.append_expect_capacity("'\\''");
        else out.push_back_expect_capacity(c);
    }
    out.push_back_expect_capacity('\'');
    return out;
}

} // uni
