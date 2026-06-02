#include <array>

namespace ayu::in {

enum CharProps : u8 {
    CHAR_ITEM_ERROR = 0,
    CHAR_ITEM_WORD = 1,
    CHAR_ITEM_DIGIT = 2,
    CHAR_ITEM_PLUS = 3,
    CHAR_ITEM_MINUS = 4,
    CHAR_ITEM_DOT = 5,
    CHAR_ITEM_STRING = 6,
    CHAR_ITEM_ARRAY = 7,
    CHAR_ITEM_OBJECT = 8,
    CHAR_ITEM_MACRO = 9,
    CHAR_ITEM_MASK = 0x0f,
    CHAR_ESCAPED = 0x10,
    CHAR_ESCAPED_EXPANDED = 0x20,
    CHAR_CONTINUES_WORD = 0x40,
    CHAR_IS_WS = 0x80,
};
constexpr std::array<u8, 256> char_props_table = []{
    std::array<u8, 256> r = {};
    for (char c = 0; c < ' '; c++) {
        r[c] = CHAR_ESCAPED | CHAR_ESCAPED_EXPANDED;
    }
    for (char c : {'\f', '\r', '\v'}) {
        r[c] = CHAR_IS_WS | CHAR_ESCAPED | CHAR_ESCAPED_EXPANDED;
    }
    for (char c : {'\n', '\t'}) {
        r[c] = CHAR_IS_WS | CHAR_ESCAPED;
    }
    r[' '] = CHAR_IS_WS;
    for (char c = '0'; c <= '9'; c++) r[c] = CHAR_ITEM_DIGIT | CHAR_CONTINUES_WORD;
    for (char c = 'a'; c <= 'z'; c++) r[c] = CHAR_ITEM_WORD | CHAR_CONTINUES_WORD;
    for (char c = 'A'; c <= 'Z'; c++) r[c] = CHAR_ITEM_WORD | CHAR_CONTINUES_WORD;
    for (char c : {
        '!','#','$','%','&','*','/','<','=','>','?','@','^','_','|','~',
    }) r[c] = CHAR_ITEM_WORD | CHAR_CONTINUES_WORD;
    r['.'] = CHAR_ITEM_DOT | CHAR_CONTINUES_WORD;
    r['+'] = CHAR_ITEM_PLUS | CHAR_CONTINUES_WORD;
    r['-'] = CHAR_ITEM_MINUS | CHAR_CONTINUES_WORD;
    r['"'] = CHAR_ITEM_STRING | CHAR_ESCAPED | CHAR_ESCAPED_EXPANDED;
    r['['] = CHAR_ITEM_ARRAY;
    r['{'] = CHAR_ITEM_OBJECT;
    r['('] = CHAR_ITEM_MACRO | CHAR_CONTINUES_WORD;
    r['\\'] = CHAR_ESCAPED | CHAR_ESCAPED_EXPANDED;
    for (int c = 128; c < 256; c++) r[c] = CHAR_ITEM_WORD | CHAR_CONTINUES_WORD;
    return r;
}();

ALWAYS_INLINE static constexpr
CharProps char_item (char c) { return CharProps(char_props_table[u8(c)] & CHAR_ITEM_MASK); }
ALWAYS_INLINE static constexpr
bool char_needs_escape (char c, bool expand) {
    return char_props_table[u8(c)] &
        (expand ? CHAR_ESCAPED_EXPANDED : CHAR_ESCAPED);
}
ALWAYS_INLINE static constexpr
bool char_continues_word (char c) { return char_props_table[u8(c)] & CHAR_CONTINUES_WORD; }
ALWAYS_INLINE static constexpr
bool char_is_ws (char c) { return char_props_table[u8(c)] & CHAR_IS_WS; }

 // Without this, the compiler makes jump tables that are more or less as big.
 // We're reusing this for both escaping and unescaping.  For escaping, it's
 // gated by char_needs_escape.  For unescaping it'll be gated by c >= 32 or
 // similar.
constexpr std::array<u8, 't' + 1> char_escape_table = []{
    std::array<u8, 't' + 1> r = {};
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
