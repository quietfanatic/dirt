#include <array>

namespace ayu::in {

enum CharProps : u8 {
    CHAR_IS_WS = 0x80,
    CHAR_CONTINUES_WORD = 0x40,
     // Only used when a word starts with ., so second candidate for eviction.
    CHAR_ILLEGAL_AFTER_DOT = 0x20,
     // Only used for error reporting, so evict this if we need another bit.
    CHAR_RESERVED = 0x10,
    CHAR_TERM_MASK = 0x0f,
    CHAR_TERM_ERROR = 0,
    CHAR_TERM_WORD = 1,
    CHAR_TERM_DIGIT = 2,
    CHAR_TERM_DOT = 3,
    CHAR_TERM_PLUS = 4,
    CHAR_TERM_MINUS = 5,
    CHAR_TERM_STRING = 6,
    CHAR_TERM_ARRAY = 7,
    CHAR_TERM_OBJECT = 8,
    CHAR_TERM_SHORTCUT = 9,
};
constexpr std::array<u8, 256> char_props_table = []{
    std::array<u8, 256> r = {};
    for (char c : {' ', '\f', '\n', '\r', '\t', '\v'}) {
        r[c] = CHAR_IS_WS;
    }
    for (char c = '0'; c <= '9'; c++) r[c] = CHAR_CONTINUES_WORD | CHAR_ILLEGAL_AFTER_DOT | CHAR_TERM_DIGIT;
    for (char c = 'a'; c <= 'z'; c++) r[c] = CHAR_CONTINUES_WORD | CHAR_TERM_WORD;
    for (char c = 'A'; c <= 'Z'; c++) r[c] = CHAR_CONTINUES_WORD | CHAR_TERM_WORD;
    for (char c : {
        '!', '$', '%', '+', '-', '.', '/', '<', '>',
        '?', '@', '^', '_', '~', '#', '&', '*'
    }) r[c] = CHAR_CONTINUES_WORD;
    for (char c : {'_', '/', '?', '#'}) r[c] |= CHAR_TERM_WORD;
    for (char c : {'\\', '`', '(', ')', '\'', ';'}) r[c] |= CHAR_RESERVED;
    r['.'] |= CHAR_TERM_DOT;
    r['+'] |= CHAR_ILLEGAL_AFTER_DOT | CHAR_TERM_PLUS;
    r['-'] |= CHAR_ILLEGAL_AFTER_DOT | CHAR_TERM_MINUS;
    r['"'] |= CHAR_TERM_STRING;
    r['['] |= CHAR_TERM_ARRAY;
    r['{'] |= CHAR_TERM_OBJECT;
    r['$'] |= CHAR_TERM_SHORTCUT;
    return r;
}();

ALWAYS_INLINE static constexpr
bool char_is_ws (char c) { return char_props_table[u8(c)] & CHAR_IS_WS; }
ALWAYS_INLINE static constexpr
bool char_continues_word (char c) { return char_props_table[u8(c)] & CHAR_CONTINUES_WORD; }
ALWAYS_INLINE static constexpr
bool char_illegal_after_dot (char c) { return char_props_table[u8(c)] & CHAR_ILLEGAL_AFTER_DOT; }
ALWAYS_INLINE static constexpr
bool char_reserved (char c) { return char_props_table[u8(c)] & CHAR_RESERVED; }
ALWAYS_INLINE static constexpr
CharProps char_term (char c) { return CharProps(char_props_table[u8(c)] & CHAR_TERM_MASK); }

} // ayu::in
