// This module has the tree <- string <- file pipeline.

#pragma once

#include "../common.h"

namespace ayu {

 // The filename parameter is used for error reporting.
 // Throws if the parse fails.
Tree tree_from_string (Str, const AnyString& filename = "");
Tree tree_from_file (MoveRef<AnyString> filename);

 // Parse multple items separated by commas or whitespace.  This is essentially
 // parsing an array, but without the surrounding [ and ].
UniqueArray<Tree> tree_list_from_string (Str, const AnyString& filename = "");
UniqueArray<Tree> tree_list_from_file (MoveRef<AnyString> filename);

constexpr ErrorCode e_ParseFailed = "ayu::e_ParseFailed";

} // namespace ayu
