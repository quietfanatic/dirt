#include <array>

namespace ayu::in {

enum CharProps : u8 {
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
    CHAR_TERM_MASK = 0x0f,
    CHAR_NEEDS_ESCAPE = 0x10,
    CHAR_ILLEGAL_AFTER_DOT = 0x20, // not often used
    CHAR_CONTINUES_WORD = 0x40,
    CHAR_IS_WS = 0x80,
};
constexpr std::array<u8, 256> char_props_table = []{
    std::array<u8, 256> r = {};
    for (char c = 0; c < ' '; c++) {
         // Whether \n and \t need to be escaped depends on whether the string
         // is printed in expanded form.  We're running out of bits, so don't
         // bother differentiating them.  Mark them as needing escape for now
         // and deal with them later.
        r[c] = CHAR_NEEDS_ESCAPE;
    }
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
    r['.'] |= CHAR_TERM_DOT;
    r['+'] |= CHAR_ILLEGAL_AFTER_DOT | CHAR_TERM_PLUS;
    r['-'] |= CHAR_ILLEGAL_AFTER_DOT | CHAR_TERM_MINUS;
    r['"'] |= CHAR_TERM_STRING | CHAR_NEEDS_ESCAPE;
    r['['] |= CHAR_TERM_ARRAY;
    r['{'] |= CHAR_TERM_OBJECT;
    r['$'] |= CHAR_TERM_SHORTCUT;
    r['\\'] |= CHAR_NEEDS_ESCAPE;
    return r;
}();

ALWAYS_INLINE static constexpr
CharProps char_term (char c) { return CharProps(char_props_table[u8(c)] & CHAR_TERM_MASK); }
ALWAYS_INLINE static constexpr
bool char_needs_escape (char c) { return char_props_table[u8(c)] & CHAR_NEEDS_ESCAPE; }
ALWAYS_INLINE static constexpr
bool char_illegal_after_dot (char c) { return char_props_table[u8(c)] & CHAR_ILLEGAL_AFTER_DOT; }
ALWAYS_INLINE static constexpr
bool char_continues_word (char c) { return char_props_table[u8(c)] & CHAR_CONTINUES_WORD; }
ALWAYS_INLINE static constexpr
bool char_is_ws (char c) { return char_props_table[u8(c)] & CHAR_IS_WS; }
static constexpr
bool char_reserved (char c) {
     // We're out of bits in the prop table and this is only used for error
     // reporting.
    return c == '\\' || c == '`' || c == '(' || c == ')' || c == '\'' || c == ';';
}

 // Without this, the compiler makes jump tables that are more or less as big.
 // We're reusing this for both escaping and unescaping.  For escaping, it's
 // gated by the CHAR_NEEDS_ESCAPE bit.  For unescaping it'll be gated by
 // c >= 32 or similar.
constexpr std::array<u8, 'x' + 1> char_escape_table = []{
    std::array<u8, 'x' + 1> r = {};
     // Escapes
    r['\b'] = 'b';
    r['\f'] = 'f';
    r['\n'] = 'n';
    r['\r'] = 'r';
    r['\t'] = 't';
     // Unescapes
    r['/'] = '/'; // Dunno why this is in JSON
    r['b'] = '\b';
    r['f'] = '\f';
    r['n'] = '\n';
    r['r'] = '\r';
    r['t'] = '\t';
     // Both
    r['"'] = '"';
    r['\\'] = '\\';
    return r;
}();

} // ayu::in
