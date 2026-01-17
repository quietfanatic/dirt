#pragma once

#include "../uni/arrays.h"

namespace control::in {
    void register_command (const void* cmd, void* registry);
    const void* lookup_command (uni::Str name, const void* registry) noexcept;
    const void* get_command (uni::Str name, const void* registry);
}
