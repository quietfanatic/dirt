#pragma once

#include "arrays.h"

namespace uni {

struct CommandResult {
    UniqueString out;
    int ret;
    bool command_wasnt_found ();
};

 // Run command through system's command interpreter.
CommandResult shell (const char* command);

 // Run a command with arguments
CommandResult run (Slice<Str> args);

 // Makes this string safe for interpolation into the command line.  Currently
 // only implemented for sh-like shells.
UniqueString escape_for_shell (Str);

} // uni
