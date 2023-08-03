#include "../parse.h"

#include <cstring>
#include <charconv>
#include <limits>

#include "../../uni/text.h"
#include "../../uni/utf.h"
#include "../print.h" // for error reporting
#include "char-cases-private.h"

namespace ayu {

namespace in {

 // Parsing is simple enough that we don't need a separate lexer step.
struct Parser {
    const char* p;
    const char* end;
    const char* begin;
    AnyString filename;

     // std::unordered_map is supposedly slow, so we'll use an array instead.
     // We'll rethink if we ever need to parse a document with a large amount
     // of shortcuts (I can't imagine for my use cases having more than 20
     // or so).
    UniqueArray<TreePair> shortcuts;

    Parser (Str s, MoveRef<AnyString> filename) :
        p(s.begin()),
        end(s.end()),
        begin(s.begin()),
        filename(*move(filename))
    { }

    [[noreturn, gnu::cold]]
    void error (Str mess) {
         // Diagnose line and column number
         // I'm not sure the col is exactly right
        uint line = 1;
        const char* last_lf = begin - 1;
        for (const char* p2 = begin; p2 != p; p2++) {
            if (*p2 == '\n') {
                line++;
                last_lf = p2;
            }
        }
        uint col = p - last_lf;
        raise(e_ParseFailed, cat(
            mess, " at ", filename, ':', line, ':', col
        ));
    }

    [[noreturn, gnu::cold]]
    void error_reserved (char sym) {
        error(cat(sym, " is a reserved symbol and can't be used outside of strings"));
    }

    ///// NON-SEMANTIC CONTENT

    void skip_comment () {
        p += 2;  // for two -s
        while (p < end) {
            if (*p++ == '\n') break;
        }
    }
    NOINLINE void skip_ws () {
        while (p < end) {
            switch (*p) {
                case ANY_WS: p++; break;
                case '-': {
                    if (p + 1 < end && p[1] == '-') {
                        skip_comment();
                        break;
                    }
                    else return;
                }
                default: return;
            }
        }
    }
    NOINLINE void skip_commas () {
        while (p < end) {
            switch (*p) {
                case ANY_WS: case ',': p++; break;
                case '-': {
                    if (p + 1 < end && p[1] == '-') {
                        skip_comment();
                        break;
                    }
                    else return;
                }
                default: return;
            }
        }
    }

    ///// STRINGS

    char got_x_escape () {
        {
            if (p + 2 >= end) goto invalid_x;
            int n0 = from_hex_digit(p[0]);
            if (n0 < 0) goto invalid_x;
            int n1 = from_hex_digit(p[1]);
            if (n1 < 0) goto invalid_x;
            p += 2;
            return n0 << 4 | n1;
        }
        invalid_x: error("Invalid \\x escape sequence");
    }

    UniqueString got_u_escape () {
        UniqueString16 units (Capacity(1));
         // Process multiple \uXXXX sequences at once so
         // that we can fuse UTF-16 surrogates.
        for (;;) {
            if (p + 4 >= end) goto invalid_u;
            int n0 = from_hex_digit(p[0]);
            if (n0 < 0) goto invalid_u;
            int n1 = from_hex_digit(p[1]);
            if (n1 < 0) goto invalid_u;
            int n2 = from_hex_digit(p[2]);
            if (n0 < 0) goto invalid_u;
            int n3 = from_hex_digit(p[3]);
            if (n1 < 0) goto invalid_u;
            units.push_back(n0 << 12 | n1 << 8 | n2 << 4 | n3);
            p += 4;
            if (p + 2 < end && p[0] == '\\' && p[1] == 'u') {
                p += 2;
            }
            else break;
        }
        return from_utf16(units);
        invalid_u: error("Invalid \\u escape sequence");
    }

    NOINLINE UniqueString got_string () {
        p++;  // for the "
        if (*p == '"') {
            p++;
            return "";
        }
        UniqueString r (Capacity(1));
        while (p < end) {
            char c = *p++;
            switch (c) {
                case '"': return r;
                case '\\': {
                    if (p >= end) goto not_terminated;
                    switch (*p++) {
                        case '"': c = '"'; break;
                        case '\\': c = '\\'; break;
                         // Dunno why this is in json
                        case '/': c = '/'; break;
                        case 'b': c = '\b'; break;
                        case 'f': c = '\f'; break;
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'x': c = got_x_escape(); break;
                        case 'u':
                            r.append(got_u_escape());
                            continue; // Skip the push_back
                        default: p--; error("Unrecognized escape sequence");
                    }
                    break;
                }
                default: [[likely]] break;
            }
            r.push_back(c);
        }
        not_terminated: error("String not terminated by end of input");
    }

    NOINLINE const char* find_word_end () {
        const char* r = p;
        while (r < end) {
            switch (*r) {
                case ANY_LETTER: case ANY_DECIMAL_DIGIT: case ANY_WORD_SYMBOL:
                    r++; break;
                case ':': {
                     // Allow :: for c++ types
                    if (r < end && r[1] == ':') {
                        r += 2;
                        break;
                    }
                    else return r;
                }
                case '"': {
                    p = r;
                    error("\" cannot occur inside a word (are you missing the first \"?)");
                }
                default: return r;
            }
        }
        return r;
    }

    Str got_word () {
        auto word_start = p;
        p++; // Already parsed initial character
        p = find_word_end();
        return expect(Str(word_start, p));
    }

    ///// NUMBERS

    [[noreturn, gnu::cold]]
    void error_invalid_number () { error("Couldn't parse number"); }

    template <bool hex>
    Tree parse_floating (const char* word_end, bool minus) {
        double floating;
        auto [num_end, ec] = std::from_chars(
            p, word_end, floating,
            hex ? std::chars_format::hex
                : std::chars_format::general
        );
        expect(num_end > p);
        if (num_end == word_end) {
            p = num_end;
            Tree r (minus ? -floating : floating);
            if (hex) r.flags |= PREFER_HEX;
            return r;
        }
        else error_invalid_number();
    }

    template <bool hex>
    Tree parse_number (const char* word_end, bool minus) {
        if (p >= word_end) error_invalid_number();
        switch (p[0]) {
            case ANY_DECIMAL_DIGIT: break;
            case ANY_HEX_LETTER: if (hex) break; else error_invalid_number();
            case '.': error("Number cannot start with .");
            default: error_invalid_number();
        }
        int64 integer;
        auto [num_end, ec] = std::from_chars(
            p, word_end, integer, hex ? 16 : 10
        );
        expect(num_end > p);
        if (num_end == word_end) {
            p = num_end;
            Tree r = minus && integer == 0
                ? Tree(-0.0)
                : Tree(minus ? -integer : integer);
            if (hex) r.flags |= PREFER_HEX;
            return r;
        }
         // Forbid . without a digit after
        else if (num_end < word_end && num_end[0] == '.') {
            if (num_end + 1 >= word_end ||
                !(hex ? std::isxdigit(num_end[1]) : std::isdigit(num_end[1]))
            ) error("Number cannot end with .");
        }
        return parse_floating<hex>(word_end, minus);
    }

    NOINLINE Tree parse_number_based (const char* word_end, bool minus) {
         // Detect hex prefix
        if (p + 1 < word_end && (Str(p, 2) == "0x" || Str(p, 2) == "0X")) {
            p += 2;
            return parse_number<true>(word_end, minus);
        }
        else return parse_number<false>(word_end, minus);
    }

    Tree got_plus () {
        auto word_start = p;
        p++;  // For the +
        auto word_end = find_word_end();
        if (Str(word_start, word_end) == "+nan") {
            p = word_end;
            return Tree(std::numeric_limits<double>::quiet_NaN());
        }
        else if (Str(word_start, word_end) == "+inf") {
            p = word_end;
            return Tree(std::numeric_limits<double>::infinity());
        }
        return parse_number_based(word_end, false);
    }

    Tree got_minus () {
        auto word_start = p;
        p++;  // For the -
        auto word_end = find_word_end();
        if (Str(word_start, word_end) == "-inf") {
            p = word_end;
            return Tree(-std::numeric_limits<double>::infinity());
        }
        return parse_number_based(word_end, true);
    }

    Tree got_digit () {
        return parse_number_based(find_word_end(), false);
    }

    ///// COMPOUND

    TreeArray got_array () {
        UniqueArray<Tree> r;
        p++;  // for the [
        while (p < end) {
            skip_commas();
            switch (*p) {
                case ':': error("Cannot have : in an array");
                case ']': p++; return r;
                default: r.push_back(parse_term()); break;
            }
        }
        error("Array is not terminated");
    }

    TreeObject got_object () {
        UniqueArray<TreePair> r;
        p++;  // for the {
        while (p < end) {
            skip_commas();
            if (p >= end) goto not_terminated;
            switch (*p) {
                case ':': error("Missing key before : in object");
                case '}': p++; return r;
                default: break;
            }
            Tree key = parse_term();
            if (key.form != STRING) {
                error("Can't use non-string as key in object");
            }
            skip_ws();
            if (p >= end) goto not_terminated;
            switch (*p) {
                case ':': p++; break;
                case ANY_RESERVED_SYMBOL: error_reserved(*p);
                default: error("Missing : after name in object");
            }
            skip_ws();
            if (p >= end) goto not_terminated;
            switch (*p) {
                case ',':
                case '}': error("Missing value after : in object");
                default: {
                    r.emplace_back(AnyString(move(key)), parse_term());
                    break;
                }
            }
        }
        not_terminated: error("Object is not terminated");
    }

    ///// SHORTCUTS

    NOINLINE AnyString parse_shortcut_name (char sigil) {
        switch (*p) {
            case ANY_WORD_STARTER: {
                Str word = got_word();
                if (word == "null" || word == "true" || word == "false") {
                    goto nope;
                }
                return StaticString(word);
            }
            case '"': return got_string(); break;
            default: goto nope;
        }
        nope: error(cat("Expected string for shortcut name after ", sigil));
    }

    NOINLINE Tree& set_shortcut (MoveRef<AnyString> name_) {
        auto name = *move(name_);
        for (auto& p : shortcuts) {
            if (p.first == name) {
                error(cat("Multiple declarations of shortcut &", move(name)));
            }
        }
        return shortcuts.emplace_back(move(name), parse_term()).second;
    }

    TreeRef get_shortcut (Str name) {
        for (auto& p : shortcuts) {
            if (p.first == name) return p.second;
        }
        error(cat("Unknown shortcut *", name));
    }

    Tree got_decl () {
        p++;  // for the &
        AnyString name = parse_shortcut_name('&');
        skip_ws();
        if (p < end && *p == ':') {
            p++;
            skip_ws();
            set_shortcut(move(name));
            skip_commas();
            return parse_term();
        }
        else {
            return set_shortcut(move(name));
        }
    }

    Tree got_shortcut () {
        p++;  // for the *
        AnyString name = parse_shortcut_name('*');
        return get_shortcut(name);
    }

    ///// TERM

    Tree parse_term () {
        if (p >= end) error("Expected term but ran into end of file");
        switch (*p) {
            case ANY_WORD_STARTER: {
                Str word = got_word();
                if (word == "null") return Tree(null);
                else if (word == "true") return Tree(true);
                else if (word == "false") return Tree(false);
                else return Tree(word);
            }

            case ANY_DECIMAL_DIGIT: return got_digit();
            case '+': return got_plus();
             // Comments starting with -- should already have been skipped by a
             // previous skip_ws().
            case '-': return got_minus();

            case '"': return Tree(got_string());
            case '[': return Tree(got_array());
            case '{': return Tree(got_object());

            case '&': return got_decl();
            case '*': return got_shortcut();

            case ':':
            case ',':
            case ']':
            case '}': error(cat("Unexpected ", *p));
            case '.': error("Number cannot begin with .");
            case ANY_RESERVED_SYMBOL: error_reserved(*p);
            default: error(cat(
                "Unrecognized character <", to_hex_digit(uint8(*p) >> 4),
                to_hex_digit(*p & 0xf), '>'
            ));
        }
    }

    ///// TOP

    Tree parse () {
         // Skip BOM
        if (p + 2 < end && Str(p, 3) == "\xef\xbb\xbf") {
            p += 3;
        }
        skip_ws();
        Tree r = parse_term();
        skip_ws();
        if (p != end) error("Extra stuff at end of document");
        return r;
    }
};

} using namespace in;

 // Finally:
Tree tree_from_string (Str s, AnyString filename) {
    return Parser(s, move(filename)).parse();
}

UniqueString string_from_file (AnyString filename) {
    FILE* f = fopen_utf8(filename.c_str(), "rb");
    if (!f) {
        raise_io_error(e_OpenFailed,
            "Failed to open for reading ", filename, errno
        );
    }

    fseek(f, 0, SEEK_END);
    usize size = ftell(f);
    rewind(f);

    char* buf = SharableBuffer<char>::allocate(size);
    usize did_read = fread(buf, 1, size, f);
    if (did_read != size) {
        int errnum = errno;
        fclose(f);
        SharableBuffer<char>::deallocate(buf);
        raise_io_error(e_ReadFailed, "Failed to read from ", filename, errnum);
    }

    if (fclose(f) != 0) {
        int errnum = errno;
        SharableBuffer<char>::deallocate(buf);
        raise_io_error(e_CloseFailed, "Failed to close ", filename, errnum);
    }
    return UniqueString::UnsafeConstructOwned(buf, size);
}

Tree tree_from_file (AnyString filename) {
    UniqueString s = string_from_file(filename);
    return tree_from_string(move(s), move(filename));
}

} using namespace ayu;

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"
static tap::TestSet tests ("dirt/ayu/parse", []{
    using namespace tap;
    auto y = [](StaticString s, const Tree& t){
        try_is<Tree>([&]{return tree_from_string(s);}, t, cat("yes: ", s));
    };
    auto n = [](StaticString s){
        throws_code<e_ParseFailed>([&]{
            tree_from_string(s);
        }, cat("no: ", s));
    };
    y("null", Tree(null));
    y("0", Tree(0));
    y("345", Tree(345));
    y("-44", Tree(-44));
    y("2.5", Tree(2.5));
    y("-4", Tree(-4.0));
    y("1e45", Tree(1e45));
    y("0xdeadbeef00", Tree(0xdeadbeef00));
    y("+0x40", Tree(0x40));
    y("-0x40", Tree(-0x40));
    y("000099", Tree(99));
    y("000", Tree(0));
    n("4.");
    n(".4");
    n("0.e4");
    y("0xdead.beefP30", Tree(0xdead.beefP30));
    y("+0xdead.beefP30", Tree(0xdead.beefP30));
    y("-0xdead.beefP30", Tree(-0xdead.beefP30));
    n("++0");
    n("-+0");
    n("+-0");
    n("--0"); // String contains nothing but a comment
    y("+nan", Tree(0.0/0.0));
    y("+inf", Tree(1.0/0.0));
    y("-inf", Tree(-1.0/0.0));
    y("\"\"", Tree(""));
    y("asdf", Tree("asdf"));
    y("\"null\"", Tree("null"));
    y("\"true\"", Tree("true"));
    y("\"false\"", Tree("false"));
    y("\"asdf\\x33asdf\"", Tree("asdf3asdf"));
    n("\"af\\x3wasdf\"");
    n("\"asdfasdf\\x");
    y("\"asdf\\u0037asdf\"", Tree("asdf7asdf"));
    y("\"asdf\\uD83C\\uDF31asdf\"", Tree("asdf🌱asdf"));
    y("[]", Tree(TreeArray{}));
    y("[,,,,,]", Tree(TreeArray{}));
    y("[0 1 foo]", Tree(TreeArray{Tree(0), Tree(1), Tree("foo")}));
    y("{}", Tree(TreeObject{}));
    y("{\"asdf\":\"foo\"}", Tree(TreeObject{TreePair{"asdf", Tree("foo")}}));
    y("{\"asdf\":0}", Tree(TreeObject{TreePair{"asdf", Tree(0)}}));
    y("{asdf:0}", Tree(TreeObject{TreePair{"asdf", Tree(0)}}));
    n("{0:0}");
    y("{a:0 \"null\":1 \"0\":foo}",
        Tree(TreeObject{
            TreePair{"a", Tree(0)},
            TreePair{"null", Tree(1)},
            TreePair{"0", Tree("foo")}
        })
    );
    y("[[0 1] [[2] [3 4]]]",
        Tree(TreeArray{
            Tree(TreeArray{Tree(0), Tree(1)}),
            Tree(TreeArray{
                Tree(TreeArray{Tree(2)}),
                Tree(TreeArray{Tree(3), Tree(4)})
            })
        })
    );
    y("&foo 1", Tree(1));
    y("&foo:1 *foo", Tree(1));
    y("&\"null\":4 *\"null\"", Tree(4));
    y("[&foo 1 *foo]", Tree(TreeArray{Tree(1), Tree(1)}));
    y("[&foo:1 *foo]", Tree(TreeArray{Tree(1)}));
    y("{&key asdf:*key}", Tree(TreeObject{TreePair{"asdf", Tree("asdf")}}));
    y("{&borp:\"bump\" *borp:*borp}", Tree(TreeObject{TreePair{"bump", Tree("bump")}}));
    y("3 --4", Tree(3));
    y("#", Tree("#"));
    y("#foo", Tree("#foo"));
    n("{&borp:44 *borp:*borp}");
    n("&foo");
    n("&foo:1");
    n("&1 1");
    n("&null 1");
    n("*foo");
    n("4 &foo:4");
    n("&foo *foo");
    n("&foo:*foo 1");
    n("&&a 1");
    n("& a 1");
    n("[+nana]");
    done_testing();
});
#endif
