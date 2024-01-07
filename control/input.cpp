#include "input.h"

#include <cstring>
#include <SDL2/SDL_events.h>
#include "../ayu/reflection/describe.h"
#include "../uni/hash.h"

namespace control {

bool input_matches_event (const Input& input, SDL_Event* event) noexcept {
    switch (event->type) {
        case SDL_KEYDOWN: {
            if (event->key.repeat && !(input.flags & InputFlags::Repeatable)) {
                return false;
            }
            if (input.code != event->key.keysym.sym) return false;
            return (!!(input.flags & InputFlags::Ctrl) ==
                    !!(event->key.keysym.mod & KMOD_CTRL))
                && (!!(input.flags & InputFlags::Alt) ==
                    !!(event->key.keysym.mod & KMOD_ALT))
                && (!!(input.flags & InputFlags::Shift) ==
                    !!(event->key.keysym.mod & KMOD_SHIFT));
        }
        case SDL_MOUSEBUTTONDOWN: {
            if (input.code != event->button.button) return false;
            return (!!(input.flags & InputFlags::Ctrl) ==
                    !!(SDL_GetModState() & KMOD_CTRL))
                && (!!(input.flags & InputFlags::Alt) ==
                    !!(SDL_GetModState() & KMOD_ALT))
                && (!!(input.flags & InputFlags::Shift) ==
                    !!(SDL_GetModState() & KMOD_SHIFT));
        }
        default: return false;
    }
}

static SDL_Event new_event () {
    SDL_Event r;
    std::memset(&r, 0, sizeof(r));
    return r;
}

static void send_key_event (int type, int code, int window) {
    SDL_Event event = new_event();
    event.type = type;
    event.key.windowID = window;
    event.key.keysym.sym = code;
    SDL_PushEvent(&event);
}

void send_input_as_event (const Input& input, int window) noexcept {
    if (!!(input.flags & InputFlags::Ctrl)) {
        send_key_event(SDL_KEYDOWN, SDLK_LCTRL, window);
    }
    if (!!(input.flags & InputFlags::Alt)) {
        send_key_event(SDL_KEYDOWN, SDLK_LALT, window);
    }
    if (!!(input.flags & InputFlags::Shift)) {
        send_key_event(SDL_KEYDOWN, SDLK_LSHIFT, window);
    }
    switch (input.type) {
        case InputType::Key: {
            auto event = new_event();
            event.type = SDL_KEYDOWN;
            event.key.windowID = window;
             // Ignore scancode for now
            event.key.keysym.sym = input.code;
            if (!!(input.flags & InputFlags::Ctrl)) {
                event.key.keysym.mod |= KMOD_LCTRL;
            }
            if (!!(input.flags & InputFlags::Alt)) {
                event.key.keysym.mod |= KMOD_LALT;
            }
            if (!!(input.flags & InputFlags::Shift)) {
                event.key.keysym.mod |= KMOD_LSHIFT;
            }
            SDL_PushEvent(&event);
            event.type = SDL_KEYUP;
            SDL_PushEvent(&event);
            break;
        }
        case InputType::Button: {
            auto event = new_event();
            event.type = SDL_MOUSEBUTTONDOWN;
            event.window.windowID = window;
            event.button.button = input.code;
            SDL_PushEvent(&event);
            event.type = SDL_MOUSEBUTTONUP;
            SDL_PushEvent(&event);
            break;
        }
        default: require(false);
    }
    if (!!(input.flags & InputFlags::Ctrl)) {
        send_key_event(SDL_KEYUP, SDLK_LCTRL, window);
    }
    if (!!(input.flags & InputFlags::Alt)) {
        send_key_event(SDL_KEYUP, SDLK_LALT, window);
    }
    if (!!(input.flags & InputFlags::Shift)) {
        send_key_event(SDL_KEYUP, SDLK_LSHIFT, window);
    }
}

Input input_from_integer (int i) noexcept {
    switch (i) {
        case 0: case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: case 9:
            return {.type = InputType::Key, .code = SDLK_0 + i};
         // SDLK_* constants have bit 30 set
        default: return {
            .type = InputType::Key,
            .code = (1<<30) | i
        };
    }
}

int input_to_integer (const Input& input) noexcept {
    if (input.type != InputType::Key) return -1;
    switch (input.code) {
        case SDLK_0: case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
        case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
            return input.code - SDLK_0;
        default: return input.code & 0x0fffffff;
    }
}

Input input_from_string (Str name) {
    switch (hash32(name)) {
#define KEY(name, sdlk) case hash32(name): return {.type = InputType::Key, .code = sdlk};
#define ALT(name, sdlk) KEY(name, sdlk)
#include "keys-table.private.h"
#undef ALT
#undef KEY
         // TODO: Put these in the keys table
        case hash32("button1"):
        case hash32("btn1"):
        case hash32("leftbutton"):
        case hash32("leftbtn"): return {.type = InputType::Button, .code = SDL_BUTTON_LEFT};
        case hash32("button2"):
        case hash32("btn2"):
        case hash32("middlebutton"):
        case hash32("middlebtn"): return {.type = InputType::Button, .code = SDL_BUTTON_MIDDLE};
        case hash32("button3"):
        case hash32("btn3"):
        case hash32("rightbutton"):
        case hash32("rightbtn"): return {.type = InputType::Button, .code = SDL_BUTTON_RIGHT};
        case hash32("button4"):
        case hash32("btn4"): return {.type = InputType::Button, .code = SDL_BUTTON_X1};
        case hash32("button5"):
        case hash32("btn5"): return {.type = InputType::Button, .code = SDL_BUTTON_X2};
         // TODO: throw exception
        default: raise(e_General, cat("Unknown input descriptor: ", name));
    }
}

Str input_to_string (const Input& input) {
    switch (input.type) {
        case InputType::None: return "none";
        case InputType::Key: {
            switch (input.code) {
#define KEY(name, sdlk) case sdlk: return name;
#define ALT(name, sdlk) // ignore alternatives
#include "keys-table.private.h"
#undef ALT
#undef KEY
                 // Not entirely sure what to do here.
                default: return "";
            }
        }
        case InputType::Button: {
            switch (input.code) {
                case SDL_BUTTON_LEFT: return "button1";
                case SDL_BUTTON_MIDDLE: return "button2";
                case SDL_BUTTON_RIGHT: return "button3";
                case SDL_BUTTON_X1: return "button4";
                case SDL_BUTTON_X2: return "button5";
                default: return "";
            }
        }
        default: require(false); return "";
    }
}

 // These are for the AYU_DESCRIBE.  We're separating them for easier debugging.
static ayu::Tree input_to_tree (const Input& input) {
    UniqueArray<ayu::Tree> a;
    if (!!(input.type == InputType::None)) return ayu::Tree(move(a));
    if (!!(input.flags & InputFlags::Repeatable)) a.emplace_back("repeatable");
    if (!!(input.flags & InputFlags::Ctrl)) a.emplace_back("ctrl");
    if (!!(input.flags & InputFlags::Alt)) a.emplace_back("alt");
    if (!!(input.flags & InputFlags::Shift)) a.emplace_back("shift");
    switch (input.type) {
        case InputType::Key: {
            switch (input.code) {
                case SDLK_0: case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
                case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                    a.emplace_back(input.code - SDLK_0);
                    break;
                default: {
                    Str name = input_to_string(input);
                    if (!name.empty()) a.emplace_back(name);
                    else a.emplace_back(input_to_integer(input));
                    break;
                }
            }
            break;
        }
        case InputType::Button: {
            Str name = input_to_string(input);
            require(!name.empty());
            a.emplace_back(name);
            break;
        }
        default: require(false);
    }
    return ayu::Tree(move(a));
}
static void input_from_tree (Input& input, const ayu::Tree& tree) {
    auto a = Slice<ayu::Tree>(tree);
    input = {};
    for (auto& e : a) {
        if (e.form == ayu::Form::Number) {
            if (input.type != InputType::None) {
                ayu::raise(ayu::e_General, "Too many descriptors for Input");
            }
            Input tmp = input_from_integer(int(e));
            input.type = tmp.type;
            input.code = tmp.code;
        }
        else {
            auto name = Str(e);
            if (name == "repeatable") input.flags |= InputFlags::Repeatable;
            else if (name == "ctrl") input.flags |= InputFlags::Ctrl;
            else if (name == "alt") input.flags |= InputFlags::Alt;
            else if (name == "shift") input.flags |= InputFlags::Shift;
            else {
                if (input.type != InputType::None) {
                    ayu::raise(ayu::e_General, "Too many descriptors for Input");
                }
                Input tmp = input_from_string(name);
                input.type = tmp.type;
                input.code = tmp.code;
            }
        }
    }
}

} using namespace control;

AYU_DESCRIBE(control::Input,
    to_tree(&input_to_tree),
    from_tree(&input_from_tree)
)

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"
#include "../ayu/traversal/from-tree.h"
#include "../ayu/traversal/to-tree.h"

static tap::TestSet tests ("dirt/control/input", []{
    using namespace tap;

    auto test2 = [](Str s, Input expect, Str s2){
        Input got;
        ayu::item_from_string(&got, s);
        is(got.type, expect.type, uni::cat(s, " - type is correct"));
        is(got.flags, expect.flags, uni::cat(s, " - flags are correct"));
        is(got.code, expect.code, uni::cat(s, " - code is correct"));
        is(ayu::item_to_string(&expect), s2, uni::cat(s, " - item_to_string"));
    };
    auto test = [&](Str s, Input expect){
        test2(s, expect, s);
    };
    test("[]", {InputType::None, InputFlags{0}, 0});
    test("[a]", {InputType::Key, InputFlags{0}, SDLK_a});
    test("[0]", {InputType::Key, InputFlags{0}, SDLK_0});
    test("[7]", {InputType::Key, InputFlags{0}, SDLK_7});
    test("[space]", {InputType::Key, InputFlags{0}, SDLK_SPACE});
    test2("[\" \"]", {InputType::Key, InputFlags{0}, SDLK_SPACE}, "[space]");
    test("[ctrl p]", {InputType::Key, InputFlags{1}, SDLK_p});
    test("[shift r]", {InputType::Key, InputFlags{4}, SDLK_r});
    test("[f11]", {InputType::Key, InputFlags{0}, SDLK_F11});
    test("[alt enter]", {InputType::Key, InputFlags{2}, SDLK_RETURN});
    test2("[alt return]", {InputType::Key, InputFlags{2}, SDLK_RETURN}, "[alt enter]");
    test("[ctrl alt shift t]", {InputType::Key, InputFlags{7}, SDLK_t});
    test2("[v alt shift ctrl]", {InputType::Key, InputFlags{7}, SDLK_v}, "[ctrl alt shift v]");
    test("[265]", {InputType::Key, InputFlags{0}, (1<<30) | 265});
    test("[ctrl 265]", {InputType::Key, InputFlags{1}, (1<<30) | 265});
    test("[shift button1]", {InputType::Button, InputFlags{4}, SDL_BUTTON_LEFT});

    done_testing();
});

#endif
