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
    AnyString filename;
    const char* begin;
    const char* p;
    const char* end;

     // std::unordered_map is supposedly slow, so we'll use an array instead.
     // We'll rethink if we ever need to parse a document with a large amount
     // of shortcuts (I can't imagine for my use cases having more than 20
     // or so).
    UniqueArray<TreePair> shortcuts;

    Parser (Str s, AnyString filename) :
        filename(move(filename)),
        begin(s.begin()),
        p(s.begin()),
        end(s.end())
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

    void skip_comment () {
        p += 2;  // for two -s
        for (; p < end; p++) {
            if (*p == '\n') {
                p++; return;
            }
        }
    }
    void skip_ws () {
        for (; p < end; p++) {
            switch (*p) {
                case ANY_WS: break;
                case '-': {
                    if (p + 1 < end && p[1] == '-') {
                        skip_comment();
                        return skip_ws();
                    }
                    else return;
                }
                default: return;
            }
        }
    }
    void skip_commas () {
        for (; p < end; p++) {
            switch (*p) {
                case ANY_WS: case ',': break;
                case '-': {
                    if (p + 1 < end && p[1] == '-') {
                        skip_comment();
                        return skip_commas();
                    }
                    else return;
                }
                default: return;
            }
        }
    }

    UniqueString got_string () {
        p++;  // for the "
        UniqueString r;
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
                        case 'x': {
                            if (p + 2 >= end) goto invalid_x;
                            int n0 = from_hex_digit(p[0]);
                            if (n0 < 0) goto invalid_x;
                            int n1 = from_hex_digit(p[1]);
                            if (n1 < 0) goto invalid_x;
                            c = n0 << 4 | n1;
                            p += 2;
                            break;
                        }
                        case 'u': {
                            UniqueString16 units;
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
                            r.append(from_utf16(units));
                            continue; // Skip the push_back
                        }
                        default: p--; error("Unrecognized escape sequence.");
                    }
                    break;
                }
                default: [[likely]] break;
            }
            r.push_back(c);
        }
        not_terminated: error("String not terminated by end of input.");
        invalid_x: error("Invalid \\x escape sequence.");
        invalid_u: error("Invalid \\u escape sequence.");
    }

    Str got_word () {
        const char* start = p;
        p++;  // For the first character
        while (p < end) {
            switch (*p) {
                case ANY_LETTER: case ANY_DECIMAL_DIGIT: case ANY_WORD_SYMBOL:
                    p++; break;
                case ':': {
                     // Allow :: for c++ types
                    if (p < end && p[1] == ':') {
                        p += 2;
                        break;
                    }
                    else goto done;
                }
                case '"': {
                    error("\" cannot occur inside a word (are you missing the first \"?)");
                }
                case ANY_RESERVED_SYMBOL: {
                    error(cat(*p, " is a reserved symbol and can't be used outside of strings."));
                }
                default: goto done;
            }
        }
        done:
        if (p - start == 2 && start[0] == '/' && start[1] == '/') {
            error("// by itself is not a valid unquoted string (comments are --, not //).");
        }
        return Str(start, p);
    }

    Tree got_number () {
        Str word = got_word();
         // Detect special numbers
        if (word.size() == 4) {
            if (word[0] == '+' && word[1] == 'n' && word[2] == 'a' && word[3] == 'n') {
                return Tree(std::numeric_limits<double>::quiet_NaN());
            }
            if (word[0] == '+' && word[1] == 'i' && word[2] == 'n' && word[3] == 'f') {
                return Tree(std::numeric_limits<double>::infinity());
            }
            if (word[0] == '-' && word[1] == 'i' && word[2] == 'n' && word[3] == 'f') {
                return Tree(-std::numeric_limits<double>::infinity());
            }
        }
         // Detect sign
        bool minus = false;
        switch (word[0]) {
            case '-':
                minus = true;
                [[fallthrough]];
            case '+':
                word = word.substr(1);
                if (word.empty() || !std::isdigit(word[0])) goto nope;
                break;
        }
         // Detect hex prefix
        bool hex;
        if (word.size() >= 2 && word[0] == '0'
         && (word[1] == 'x' || word[1] == 'X')
        ) {
            hex = true;
            word = word.substr(2);
        }
        else hex = false;
         // Try integer
        {
            int64 integer;
            auto [ptr, ec] = std::from_chars(
                word.begin(), word.end(), integer, hex ? 16 : 10
            );
            if (ptr == word.begin()) {
                 // If the integer parse failed, the float parse will also fail.
                goto nope;
            }
            else if (ptr == word.end()) {
                Tree r = minus && integer == 0
                    ? Tree(-0.0)
                    : Tree(minus ? -integer : integer);
                if (hex) r.flags |= PREFER_HEX;
                return r;
            }
             // Forbid . without a digit after
            else if (ptr < word.end() && ptr[0] == '.') {
                if (ptr == word.end() - 1 ||
                    (hex ? !std::isxdigit(ptr[1]) : !std::isdigit(ptr[1]))
                ) error("Number cannot end with .");
            }
        }
         // Integer parse didn't take the whole word, try float parse
        {
            double floating;
            auto [ptr, ec] = std::from_chars(
                word.begin(), word.end(), floating,
                hex ? std::chars_format::hex
                    : std::chars_format::general
            );
            if (ptr == word.begin()) {
                 // Shouldn't happen?
                 goto nope;
            }
            else if (ptr == word.end()) {
                Tree r (minus ? -floating : floating);
                if (hex) r.flags |= PREFER_HEX;
                return r;
            }
            else goto nope;
        }
        nope: error("Couldn't parse number.");
    }

    TreeArray got_array () {
        UniqueArray<Tree> r;
        p++;  // for the [
        while (p < end) {
            skip_commas();
            switch (*p) {
                case ':': error("Cannot have : in an array.");
                case ']': p++; return r;
                default: r.push_back(parse_term()); break;
            }
        }
        error("Array is not terminated.");
    }

    TreeObject got_object () {
        UniqueArray<TreePair> r;
        p++;  // for the {
        while (p < end) {
            skip_commas();
            if (p >= end) goto not_terminated;
            switch (*p) {
                case ':': error("Missing key before : in object.");
                case '}': p++; return r;
                default: break;
            }
            Tree key = parse_term();
            if (key.form != STRING) {
                error("Can't use non-string as key in object.");
            }
            skip_ws();
            if (p >= end) goto not_terminated;
            switch (*p) {
                case ':': p++; break;
                case ANY_RESERVED_SYMBOL: {
                    error(cat(*p, " is a reserved symbol and can't be used outside of strings."));
                }
                default: error("Missing : after name in object.");
            }
            skip_ws();
            if (p >= end) goto not_terminated;
            switch (*p) {
                case ',':
                case '}': error("Missing value after : in object.");
                default: {
                    r.emplace_back(AnyString(move(key)), parse_term());
                    break;
                }
            }
        }
        not_terminated: error("Object is not terminated.");
    }

    void set_shortcut (AnyString&& name, Tree value) {
        for (auto& p : shortcuts) {
            if (p.first == name) {
                error(cat("Duplicate declaration of shortcut &", name));
            }
        }
        shortcuts.emplace_back(move(name), move(value));
    }
    TreeRef get_shortcut (Str name) {
        for (auto& p : shortcuts) {
            if (p.first == name) return p.second;
        }
        error(cat("Unknown shortcut *", name));
    }

    Tree got_decl () {
        p++;  // for the &
        Tree name_t = parse_term();
        if (name_t.form != STRING) {
            error("Can't use non-string as shortcut name.");
        }
        auto name = AnyString(move(name_t));
        skip_ws();
        if (p < end && *p == ':') {
            p++;
            skip_ws();
            Tree value = parse_term();
            set_shortcut(move(name), move(value));
            skip_commas();
            return parse_term();
        }
        else {
            Tree value = parse_term();
            set_shortcut(move(name), value);
            return value;
        }
    }

    Tree got_shortcut () {
        p++;  // for the *
        Tree name = parse_term();
        if (name.form != STRING) {
            error("Can't use non-string as shortcut name.");
        }
        return get_shortcut(Str(name));
    }

    Tree parse_term () {
        if (p >= end) error("Expected term but ran into end of file.");
        switch (*p) {
            case ANY_WORD_STARTER: {
                Str word = got_word();
                if (word.size() == 4) {
                    if (word[0] == 'n' && word[1] == 'u' &&
                        word[2] == 'l' && word[3] == 'l'
                    ) return Tree(null);
                    if (word[0] == 't' && word[1] == 'r' &&
                        word[2] == 'u' && word[3] == 'e'
                    ) return Tree(true);
                }
                if (word.size() == 5 && word[0] == 'f' &&
                    word[1] == 'a' && word[2] == 'l' &&
                    word[3] == 's' && word[4] == 'e'
                ) return Tree(false);
                return Tree(word);
            }

            case ANY_DECIMAL_DIGIT:
            case '+':
             // Comments starting with -- should already have been skipped by a
             // previous skip_ws().
            case '-': return got_number();

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
            case ANY_RESERVED_SYMBOL: error(cat(
                *p, " is a reserved symbol and can't be used outside of strings."
            ));
            default: error(cat(
                "Unrecognized character <", to_hex_digit(uint8(*p) >> 4),
                to_hex_digit(*p & 0xf), '>'
            ));
        }
    }

    Tree parse () {
         // Skip BOM
        if (p + 2 < end && p[0] == char(0xef)
                        && p[1] == char(0xbb)
                        && p[2] == char(0xbf)
        ) p += 3;
        skip_ws();
        Tree r = parse_term();
        skip_ws();
        if (p != end) error("Extra stuff at end of document.");
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
        int errnum = errno;
        raise(e_OpenFailed, cat(
            "Failed to open for reading ", filename, ": ", std::strerror(errnum)
        ));
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
        raise(e_ReadFailed, cat(
            "Failed to read from ", filename, ": ", std::strerror(errnum)
        ));
    }

    if (fclose(f) != 0) {
        int errnum = errno;
        SharableBuffer<char>::deallocate(buf);
        raise(e_CloseFailed, cat(
            "Failed to close ", filename, ": ", std::strerror(errnum)
        ));
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
