// Provides a class representing keyboard and mouse button inputs.
// Primarily for use with ayu

#pragma once

#include "../ayu/common.h"
#include "../uni/common.h"

union SDL_Event;

namespace control {
using namespace uni;

enum class InputType : u8 {
    None,
    Key,  // Use SDLK_* values
    Button  // USE SDL_BUTTON_* values
};

enum class InputFlags : u8 {
    Ctrl = 1,
    Alt = 2,
    Shift = 4,
    Repeatable = 8,
     // Not serializable, only set on inputs created with input_from_event.
    Repeated = 16,
};
DECLARE_ENUM_BITWISE_OPERATORS(InputFlags)

 // Input serializes to an array containing any of ctrl, alt, shift, repeatable,
 // and exactly one of whatever comes from input_to_string or input_to_integer
 // (with an exception for InputType::None serializing to an empty array).
struct Input {
    InputType type = InputType::None;
    InputFlags flags = InputFlags{0};
    i32 code = 0;
    constexpr explicit operator bool () const { return type != InputType::None; }
     // Don't use this to check inputs against bindings, the Repeatable flag
     // will mess things up.  Use input_matches_binding instead.
    friend bool operator== (Input, Input) = default;
};

 // An alternative version of Input that deserializes from a plain string or
 // integer instead of an array.  InputType::None cannot be serialized.
struct InputNoModifiers : Input { };

Input input_from_event (SDL_Event*) noexcept;
bool input_matches_binding (Input got, Input binding) noexcept;

 // Only works for InputType::Key.  Ignores modifiers!
bool input_currently_pressed (Input input) noexcept;
 // To not duplicate calls to SDL_GetKeyboardState
bool input_currently_pressed (Input input, const u8* keyboard) noexcept;

 // Mainly for testing
void send_input_as_event (Input i, int windowID) noexcept;

 // 0..9 map to the number keys, and other numbers are raw scancodes.
 // Does not work for mouse buttons.
Input input_from_integer (i32 d) noexcept;
i32 input_to_integer (Input i) noexcept;

 // Symbolic name in all lowercase (Ignores modifier keys).
 // May not work on obscure keys.
Input input_from_string (Str c);
StaticString input_to_string (Input i);

} // namespace control
