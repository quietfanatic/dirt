#pragma once

#include "../uni/arrays.h"

namespace cmd::in {
    using namespace uni;
    void register_command (void* registry, const void* cmd) noexcept;
    const void* lookup_command (const void* registry, Str name) noexcept;
    const void* get_command (const void* registry, Str name);
}
