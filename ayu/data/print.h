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

UniqueString tree_to_string (TreeRef, PrintOptions opts = {});
 // TODO: tree_to_list_string

constexpr ErrorCode e_PrintOptionsInvalid = "ayu::e_PrintOptionsInvalid";

void tree_to_file (TreeRef, AnyString filename, PrintOptions opts = {});

} // namespace ayu

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

template <>
struct tap::Show<ayu::Tree> {
    std::string show (const ayu::Tree& t) const {
        return tree_to_string(t, ayu::PrintOptions::Compact);
    }
};

#endif
