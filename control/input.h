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

struct Input {
    InputType type = InputType::None;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    int32 code = 0;
};

bool input_matches_event (const Input& i, SDL_Event* event) noexcept;

 // Mainly for testing
void send_input_as_event (const Input& i, int windowID) noexcept;

 // 0..9 map to the number keys, and other numbers are raw scancodes.
 // Does not work for mouse buttons.
Input input_from_integer (int d) noexcept;
int input_to_integer (const Input& i) noexcept;

 // Symbolic name in all lowercase (Ignores modifier keys).
 // May not work on obscure keys.
Input input_from_string (Str c);
Str input_to_string (const Input& i);

} // namespace control
