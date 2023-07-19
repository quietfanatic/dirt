// This module has the tree <- string <- file pipeline.

#pragma once

#include "common.h"

namespace ayu {

 // The filename parameter is used for error reporting.
 // If the parse fails, an X<ParseError> exception will be thrown.
Tree tree_from_string (Str, AnyString filename = "");

UniqueString string_from_file (AnyString filename);

Tree tree_from_file (AnyString filename);

constexpr ErrorCode e_ParseFailed = "ParseFailed";

} // namespace ayu
