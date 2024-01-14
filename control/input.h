// Provides a class representing keyboard and mouse button inputs.
// Primarily for use with ayu

#pragma once

#include "../ayu/common.h"
#include "../uni/common.h"

union SDL_Event;

namespace control {
using namespace uni;

enum class InputType : uint8 {
    None,
    Key,  // Use SDLK_* values
    Button  // USE SDL_BUTTON_* values
};

enum class InputFlags : uint8 {
    Ctrl = 1,
    Alt = 2,
    Shift = 4,
    Repeatable = 8,
     // Not serializable, only set on inputs created with input_from_event.
    Repeated = 16,
};
DECLARE_ENUM_BITWISE_OPERATORS(InputFlags)

struct Input {
    InputType type = InputType::None;
    InputFlags flags = InputFlags{0};
    int32 code = 0;
    constexpr operator bool () const { return type != InputType::None; }
     // Don't use this to check inputs against bindings, the Repeatable flag
     // will mess things up.  Use input_matches_binding instead.
    friend bool operator== (Input, Input) = default;
};

Input input_from_event (SDL_Event*) noexcept;
bool input_matches_binding (Input got, Input binding) noexcept;

 // Mainly for testing
void send_input_as_event (Input i, int windowID) noexcept;

 // 0..9 map to the number keys, and other numbers are raw scancodes.
 // Does not work for mouse buttons.
Input input_from_integer (int d) noexcept;
int input_to_integer (Input i) noexcept;

 // Symbolic name in all lowercase (Ignores modifier keys).
 // May not work on obscure keys.
Input input_from_string (Str c);
StaticString input_to_string (Input i);

} // namespace control
