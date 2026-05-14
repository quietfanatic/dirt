// This module has the tree <- string <- file pipeline.

#pragma once

#include "../common.h"

namespace ayu {

 // The filename parameter is used for error reporting.
 // Throws if the parse fails.
Tree tree_from_string (Str);
Tree tree_from_file (const char* filename);
Tree tree_from_file (Str filename);

 // Parse multple items separated by commas or whitespace.  This is essentially
 // parsing an array, but without the surrounding [ and ].
UniqueArray<Tree> tree_list_from_string (Str);
UniqueArray<Tree> tree_list_from_file (const char* filename);
UniqueArray<Tree> tree_list_from_file (Str filename);

constexpr ErrorCode e_ParseFailed = "ayu::e_ParseFailed";

} // namespace ayu
