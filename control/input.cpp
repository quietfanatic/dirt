#include "input.h"

#include <cstring>
#include <SDL2/SDL_events.h>
#include "../ayu/reflection/describe.h"
#include "../uni/hash.h"

namespace control {

bool input_matches_event (const Input& input, SDL_Event* event) noexcept {
    switch (event->type) {
        case SDL_KEYDOWN: {
            if (event->key.repeat) return false;
            return input.code == event->key.keysym.sym
                && (input.ctrl == !!(event->key.keysym.mod & KMOD_CTRL))
                && (input.alt == !!(event->key.keysym.mod & KMOD_ALT))
                && (input.shift == !!(event->key.keysym.mod & KMOD_SHIFT));
        }
        case SDL_MOUSEBUTTONDOWN: {
            return input.code == event->button.button
                && (input.ctrl == !!(SDL_GetModState() & KMOD_CTRL))
                && (input.alt == !!(SDL_GetModState() & KMOD_ALT))
                && (input.shift == !!(SDL_GetModState() & KMOD_SHIFT));
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
    if (input.ctrl) send_key_event(SDL_KEYDOWN, SDLK_LCTRL, window);
    if (input.alt) send_key_event(SDL_KEYDOWN, SDLK_LALT, window);
    if (input.shift) send_key_event(SDL_KEYDOWN, SDLK_LSHIFT, window);
    switch (input.type) {
        case KEY: {
            auto event = new_event();
            event.type = SDL_KEYDOWN;
            event.key.windowID = window;
             // Ignore scancode for now
            event.key.keysym.sym = input.code;
            event.key.keysym.mod = input.ctrl * KMOD_LCTRL
                                | input.alt * KMOD_LALT
                                | input.shift * KMOD_LSHIFT;
            SDL_PushEvent(&event);
            event.type = SDL_KEYUP;
            SDL_PushEvent(&event);
            break;
        }
        case BUTTON: {
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
    if (input.shift) send_key_event(SDL_KEYUP, SDLK_LSHIFT, window);
    if (input.alt) send_key_event(SDL_KEYUP, SDLK_LALT, window);
    if (input.ctrl) send_key_event(SDL_KEYUP, SDLK_LCTRL, window);
}

Input input_from_integer (int i) noexcept {
    switch (i) {
        case 0: case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: case 9:
            return {.type = KEY, .code = SDLK_0 + i};
         // SDLK_* constants have bit 30 set
        default: return {
            .type = KEY,
            .code = (1<<30) | i
        };
    }
}

int input_to_integer (const Input& input) noexcept {
    if (input.type != KEY) return -1;
    switch (input.code) {
        case SDLK_0: case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
        case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
            return input.code - SDLK_0;
        default: return input.code & 0x0fffffff;
    }
}

Input input_from_string (Str name) {
    switch (hash32(name)) {
#define KEY(name, sdlk) case hash32(name): return {.type = KEY, .code = sdlk};
#define ALT(name, sdlk) KEY(name, sdlk)
#include "keys-table.private.h"
#undef ALT
#undef KEY
         // TODO: Put these in the keys table
        case hash32("button1"):
        case hash32("btn1"):
        case hash32("leftbutton"):
        case hash32("leftbtn"): return {.type = BUTTON, .code = SDL_BUTTON_LEFT};
        case hash32("button2"):
        case hash32("btn2"):
        case hash32("middlebutton"):
        case hash32("middlebtn"): return {.type = BUTTON, .code = SDL_BUTTON_MIDDLE};
        case hash32("button3"):
        case hash32("btn3"):
        case hash32("rightbutton"):
        case hash32("rightbtn"): return {.type = BUTTON, .code = SDL_BUTTON_RIGHT};
        case hash32("button4"):
        case hash32("btn4"): return {.type = BUTTON, .code = SDL_BUTTON_X1};
        case hash32("button5"):
        case hash32("btn5"): return {.type = BUTTON, .code = SDL_BUTTON_X2};
         // TODO: throw exception
        default: return {};
    }
}

Str input_to_string (const Input& input) {
    switch (input.type) {
        case NONE: return "none";
        case KEY: {
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
        case BUTTON: {
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
    if (input.type == NONE) return ayu::Tree(move(a));
    if (input.ctrl) a.emplace_back("ctrl");
    if (input.alt) a.emplace_back("alt");
    if (input.shift) a.emplace_back("shift");
    switch (input.type) {
        case KEY: {
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
        case BUTTON: {
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
            if (input.type != NONE) {
                ayu::raise(ayu::e_General, "Too many descriptors for Input");
            }
            Input tmp = input_from_integer(int(e));
            input.type = tmp.type;
            input.code = tmp.code;
        }
        else {
            auto name = Str(e);
            if (name == "ctrl") input.ctrl = true;
            else if (name == "alt") input.alt = true;
            else if (name == "shift") input.shift = true;
            else {
                if (input.type != NONE) {
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
        is(got.ctrl, expect.ctrl, uni::cat(s, " - ctrl is correct"));
        is(got.alt, expect.alt, uni::cat(s, " - alt is correct"));
        is(got.shift, expect.shift, uni::cat(s, " - shift is correct"));
        is(got.code, expect.code, uni::cat(s, " - code is correct"));
        is(ayu::item_to_string(&expect), s2, uni::cat(s, " - item_to_string"));
    };
    auto test = [&](Str s, Input expect){
        test2(s, expect, s);
    };
    test("[]", {NONE, 0, 0, 0, 0});
    test("[a]", {KEY, 0, 0, 0, SDLK_a});
    test("[0]", {KEY, 0, 0, 0, SDLK_0});
    test("[7]", {KEY, 0, 0, 0, SDLK_7});
    test("[space]", {KEY, 0, 0, 0, SDLK_SPACE});
    test2("[\" \"]", {KEY, 0, 0, 0, SDLK_SPACE}, "[space]");
    test("[ctrl p]", {KEY, 1, 0, 0, SDLK_p});
    test("[shift r]", {KEY, 0, 0, 1, SDLK_r});
    test("[f11]", {KEY, 0, 0, 0, SDLK_F11});
    test("[alt enter]", {KEY, 0, 1, 0, SDLK_RETURN});
    test2("[alt return]", {KEY, 0, 1, 0, SDLK_RETURN}, "[alt enter]");
    test("[ctrl alt shift t]", {KEY, 1, 1, 1, SDLK_t});
    test2("[v alt shift ctrl]", {KEY, 1, 1, 1, SDLK_v}, "[ctrl alt shift v]");
    test("[265]", {KEY, 0, 0, 0, (1<<30) | 265});
    test("[ctrl 265]", {KEY, 1, 0, 0, (1<<30) | 265});
    test("[shift button1]", {BUTTON, 0, 0, 1, SDL_BUTTON_LEFT});

    done_testing();
});

#endif
