// This module contains functions for file IO

#pragma once

#include "common.h"
#include "errors.h"
#include "strings.h"

namespace uni {

UniqueString string_from_file (AnyString filename);

void string_to_file (Str, AnyString filename);

constexpr ErrorCode e_OpenFailed = "uni::e_OpenFailed";
constexpr ErrorCode e_ReadFailed = "uni::e_ReadFailed";
constexpr ErrorCode e_WriteFailed = "uni::e_WriteFailed";
constexpr ErrorCode e_CloseFailed = "uni::e_CloseFailed";

} // namespace ayu
