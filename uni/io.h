// This module contains functions for file IO

#pragma once

#include "common.h"
#include "errors.h"
#include "strings.h"

namespace uni {

UniqueString string_from_file (AnyString filename);

void string_to_file (Str, AnyString filename);

constexpr ErrorCode e_OpenFailed = "OpenFailed";
constexpr ErrorCode e_ReadFailed = "ReadFailed";
constexpr ErrorCode e_WriteFailed = "WriteFailed";
constexpr ErrorCode e_CloseFailed = "CloseFailed";

} // namespace ayu
