// This module has the tree -> string -> file pipeline

#pragma once

#include "../common.h"
#include "tree.h"

namespace ayu {

enum class PrintOptions {
     // Print with a compact layout.  This is the default for tree_to_string.
    Compact = 1 << 0,
     // Print with a pretty layout.  This is the default for tree_to_file.
    Pretty = 1 << 1,
     // Print in JSON-compatible format.  This option is NOT WELL TESTED so it
     // may produce non-conforming output.
    Json = 1 << 2,
     // For validation
    ValidBits = Compact | Pretty | Json
};
DECLARE_ENUM_BITWISE_OPERATORS(PrintOptions)

 // Writes tree to string.  May be overallocated because it assumes you won't
 // keep the string around for long.  If you're keeping multiple of these around
 // for a long time, you may want to call shrink_to_fit() on them.
UniqueString tree_to_string (const Tree&, PrintOptions opts = {});
 // TODO: tree_to_list_string

void tree_to_file (const Tree&, AnyString filename, PrintOptions opts = {});

 // Like tree_to_string but uses defaults optimized for tree_to_file.
UniqueString tree_to_string_for_file (const Tree&, PrintOptions opts = {});

constexpr ErrorCode e_PrintOptionsInvalid = "ayu::e_PrintOptionsInvalid";

} // namespace ayu

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

template <>
struct tap::Show<ayu::Tree> {
    uni::UniqueString show (const ayu::Tree& t) const {
        return tree_to_string(t, ayu::PrintOptions::Compact);
    }
};

#endif
