#include "input.h"

#include <cstring>
#include <SDL2/SDL_events.h>
#include "../ayu/reflection/describe.h"
#include "../uni/hash.h"

namespace control {

Input input_from_event (SDL_Event* event) noexcept {
    Input r;
    uint16 mods;
    switch (event->type) {
        case SDL_KEYDOWN: {
            r.type = InputType::Key;
            if (event->key.repeat) r.flags |= InputFlags::Repeated;
            mods = event->key.keysym.mod;
            r.code = event->key.keysym.sym;
            break;
        }
        case SDL_MOUSEBUTTONDOWN: {
            r.type = InputType::Button;
            mods = SDL_GetModState();
            r.code = event->button.button;
            break;
        }
        default: return r;
    }
    if (mods & KMOD_CTRL) r.flags |= InputFlags::Ctrl;
    if (mods & KMOD_ALT) r.flags |= InputFlags::Alt;
    if (mods & KMOD_SHIFT) r.flags |= InputFlags::Shift;
    return r;
}

bool input_matches_binding (Input got, Input binding) noexcept {
    if (!!(binding.flags & InputFlags::Repeatable)) {
        got.flags &= ~InputFlags::Repeated;
    }
    binding.flags &= ~InputFlags::Repeatable;
    return got == binding;
}

bool input_currently_pressed (Input input) noexcept {
    return input_currently_pressed(input, SDL_GetKeyboardState(null));
}

bool input_currently_pressed (Input input, const uint8* keyboard) noexcept {
    switch (input.type) {
        case InputType::Key: {
            return keyboard[SDL_GetScancodeFromKey(input.code)];
        }
        case InputType::Button: {
            raise(e_General, "input_currently_pressed with InputType::Button is NYI");
        }
        case InputType::None: return false;
        default: never();
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

void send_input_as_event (Input input, int window) noexcept {
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

Input input_from_integer (int32 i) noexcept {
    switch (i) {
        case 0: case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: case 9:
            return {.type = InputType::Key, .code = SDLK_0 + i};
         // SDLK_* constants have bit 30 set
        default: return {
            .type = InputType::Key,
            .code = 1<<30 | i
        };
    }
}

int32 input_to_integer (Input input) noexcept {
    if (input.type != InputType::Key) return -1;
    switch (input.code) {
        case SDLK_0: case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
        case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
            return input.code - SDLK_0;
        default: return input.code & 0x0fffffff;
    }
}

// Make a sorted table of (hashed) input names for a binary search.  We could
// make an open-addressing hash table instead, but that would take more space,
// and efficiency isn't that important here; this is just for deserialization,
// not for ordinary runtime lookups.
struct TableEntry {
    uint32 hash;
    uint32 size;
    const char* name;
    Input input;
};

static constexpr auto inputs_by_hash = []{
    constexpr TableEntry unsorted [] = {
#define KEY(n, c) {hash32(n), std::strlen(n), n, {.type = InputType::Key, .code = c}},
#define ALT(n, c) KEY(n, c)
#define BTN(n, c) {hash32(n), std::strlen(n), n, {.type = InputType::Button, .code = c}},
#define BTN_ALT(n, c) BTN(n, c)
#include "keys-table.h"
    };
    constexpr usize len = sizeof(unsorted) / sizeof(unsorted[0]);
    std::array<TableEntry, len> r;
    for (usize i = 0; i < len; i++) {
        r[i] = unsorted[i];
    }
    std::sort(r.begin(), r.end(), [](const auto& a, const auto& b){
        return a.hash < b.hash;
    });
    return r;
}();

 // Now we want to make a table mapping codes to names, but first we have to
 // determine what the ranges of codes actually are.
struct CodeRanges {
    int32 min_low = 0x7fffffff;
    int32 max_low = 0;
    int32 min_high = 0x7fffffff;
    int32 max_high = 0;
    int32 min_btn = 0x7fffffff;
    int32 max_btn = 0;
};

static constexpr CodeRanges code_ranges = []{
    CodeRanges r;
    for (auto& entry : inputs_by_hash) {
        auto code = entry.input.code;
        if (entry.input.type == InputType::Key) {
            if (code & 1<<30) {
                if (code < r.min_high) r.min_high = code;
                if (code > r.max_high) r.max_high = code;
            }
            else {
                if (code < r.min_low) r.min_low = code;
                if (code > r.max_low) r.max_low = code;
            }
        }
        else if (entry.input.type == InputType::Button) {
            if (code < r.min_btn) r.min_btn = code;
            if (code > r.max_btn) r.max_btn = code;
        }
    }
    return r;
}();
 // Assert some sane size limits
static_assert(code_ranges.max_low - code_ranges.min_low < 1000);
static_assert(code_ranges.max_high - code_ranges.min_high < 1000);
static_assert(code_ranges.max_btn - code_ranges.min_btn < 10);

 // Now make the tables.  Since we already have all the info in the
 // inputs_by_hash table, let's just index that with 8-bit indexes.
static_assert(inputs_by_hash.size() < 255);
struct InputsByCode {
    uint8 low [code_ranges.max_low - code_ranges.min_low + 1];
    uint8 high [code_ranges.max_high - code_ranges.min_high + 1];
    uint8 btn [code_ranges.max_btn - code_ranges.min_btn + 1];
};
static constexpr auto inputs_by_code = []{
    InputsByCode r;
    for (auto& i : r.low) i = -1;
    for (auto& i : r.high) i = -1;
    for (auto& i : r.btn) i = -1;
    constexpr uint32 keys [] = {
#define KEY(n, c) hash32(n),
#include "keys-table.h"
    };
    for (uint32 hash : keys) {
        for (usize i = 0; i < inputs_by_hash.size(); i++) {
            if (inputs_by_hash[i].input.type == InputType::Key) {
                if (hash == inputs_by_hash[i].hash) {
                    auto code = inputs_by_hash[i].input.code;
                    if (code & 1<<30) r.high[code - code_ranges.min_high] = i;
                    else r.low[code - code_ranges.min_low] = i;
                    break;
                }
            }
        }
    }
    constexpr uint32 btns [] = {
#define BTN(n, c) hash32(n),
#include "keys-table.h"
    };
    for (uint32 hash : btns) {
        for (usize i = 0; i < inputs_by_hash.size(); i++) {
            if (inputs_by_hash[i].input.type == InputType::Button) {
                if (hash == inputs_by_hash[i].hash) {
                    auto code = inputs_by_hash[i].input.code;
                    r.btn[code - code_ranges.min_btn] = i;
                    break;
                }
            }
        }
    }
    return r;
}();
 // For some reason, when accessing members of a global struct constant, the
 // compiler generates code that loads the address of the struct and then adds
 // the offset to the member, instead of loading the address of the member
 // directly.  This works around that.
static constexpr StaticArray<uint8> inputs_by_code_low (inputs_by_code.low);
static constexpr StaticArray<uint8> inputs_by_code_high (inputs_by_code.high);
static constexpr StaticArray<uint8> inputs_by_code_btn (inputs_by_code.btn);

Input input_from_string (Str name) {
    if (!name) return Input{};
    if (name.size() > 32) {
        raise(e_General, "Input descriptor is too long to be an input name");
    }
    uint32 hash = hash32(name);
     // Binary search.  <algorithm> only has a binary search that returns a bool
     // and one that returns a range of elements, and both require an input
     // that's the same type as a table entry.
     //
     // Using integers here instead of pointers seems to optimize better.
    uint32 b = 0;
    uint32 e = inputs_by_hash.size();
    for (;;) {
        uint32 mid = b + (e - b) / 2;
        auto& entry = inputs_by_hash[mid];
        if (hash == entry.hash) {
            if (name == Str(entry.name, entry.size)) {
                return entry.input;
            }
            else break;
        }
        else if (b == mid) break;
        else if (hash < entry.hash) e = mid;
        else b = mid;
    }
    raise(e_General, cat("Unknown input descriptor: ", name));
}

StaticString input_to_string (Input input) {
    switch (input.type) {
        case InputType::None: return "none";
        case InputType::Key: {
            if (input.code & 1<<30) {
                if (input.code < code_ranges.min_high ||
                    input.code > code_ranges.max_high
                ) return "";
                auto ii = input.code - code_ranges.min_high;
                auto i = inputs_by_code_high[ii];
                if (i == uint8(-1)) return "";
                return StaticString(
                    inputs_by_hash[i].name, inputs_by_hash[i].size
                );
            }
            else {
                if (input.code < code_ranges.min_low ||
                    input.code > code_ranges.max_low
                ) return "";
                auto ii = input.code - code_ranges.min_low;
                auto i = inputs_by_code_low[ii];
                if (i == uint8(-1)) return "";
                return StaticString(
                    inputs_by_hash[i].name, inputs_by_hash[i].size
                );
            }
        }
        case InputType::Button: {
            if (input.code < code_ranges.min_btn ||
                input.code > code_ranges.max_btn
            ) return "";
            auto ii = input.code - code_ranges.min_btn;
            auto i = inputs_by_code_btn[ii];
            if (i == uint8(-1)) return "";
            return StaticString(
                inputs_by_hash[i].name, inputs_by_hash[i].size
            );
        }
        default: require(false); return "";
    }
}

 // These are for the AYU_DESCRIBE.  We're separating them for easier debugging.
static ayu::Tree input_to_tree (const Input& input) {
    if (!!(input.type == InputType::None)) return ayu::Tree::array();
    auto a = UniqueArray<ayu::Tree>(
        Capacity(1 + std::popcount(uint8(input.flags)))
    );
    if (!!(input.flags & InputFlags::Repeatable)) {
        a.emplace_back_expect_capacity("repeatable");
    }
    if (!!(input.flags & InputFlags::Ctrl)) {
        a.emplace_back_expect_capacity("ctrl");
    }
    if (!!(input.flags & InputFlags::Alt)) {
        a.emplace_back_expect_capacity("alt");
    }
    if (!!(input.flags & InputFlags::Shift)) {
        a.emplace_back_expect_capacity("shift");
    }
    switch (input.type) {
        case InputType::Key: {
            switch (input.code) {
                case SDLK_0: case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
                case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                    a.emplace_back_expect_capacity(input.code - SDLK_0);
                    break;
                default: {
                    StaticString name = input_to_string(input);
                    if (!name.empty()) a.emplace_back_expect_capacity(name);
                    else a.emplace_back_expect_capacity(input_to_integer(input));
                    break;
                }
            }
            break;
        }
        case InputType::Button: {
            StaticString name = input_to_string(input);
            expect(name);
            a.emplace_back_expect_capacity(name);
            break;
        }
        default: never();
    }
    return ayu::Tree(move(a));
}
static void input_from_tree (Input& input, const ayu::Tree& tree) {
    auto a = Slice<ayu::Tree>(tree);
    input = {};
    if (!a) return;
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
    if (input.type == InputType::None) {
        ayu::raise(ayu::e_General, "Input has modifiers but no actual code");
    }
}

static ayu::Tree input_to_tree_no_modifiers (const InputNoModifiers& input) {
    switch (input.type) {
        case InputType::None: return ayu::Tree("");
        case InputType::Key: {
            switch (input.code) {
                case SDLK_0: case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
                case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                    return ayu::Tree(input.code - SDLK_0);
                default: {
                    StaticString name = input_to_string(input);
                    if (!name.empty()) return ayu::Tree(name);
                    else return ayu::Tree(input_to_integer(input));
                }
            }
            break;
        }
        case InputType::Button: {
            StaticString name = input_to_string(input);
            return ayu::Tree(expect(name));
        }
        default: never();
    }
}

static void input_from_tree_no_modifiers (InputNoModifiers& input, const ayu::Tree& tree) {
    switch (tree.form) {
        case ayu::Form::String: {
            input = InputNoModifiers(input_from_string(Str(tree)));
            break;
        }
        case ayu::Form::Number: {
            input = InputNoModifiers(input_from_integer(int32(tree)));
            break;
        }
        default: ayu::raise(e_General, "InputNoModifiers wasn't given a string or integer");
    }
}

} using namespace control;

AYU_DESCRIBE(control::Input,
    to_tree(&input_to_tree),
    from_tree(&input_from_tree)
)

AYU_DESCRIBE(control::InputNoModifiers,
    to_tree(&input_to_tree_no_modifiers),
    from_tree(&input_from_tree_no_modifiers)
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
