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
    const char* end;
    const char* begin;
    AnyString filename;

     // std::unordered_map is supposedly slow, so we'll use an array instead.
     // We'll rethink if we ever need to parse a document with a large amount
     // of shortcuts (I can't imagine for my use cases having more than 20
     // or so).
    UniqueArray<TreePair> shortcuts;

    Parser (Str s, MoveRef<AnyString> filename) :
        end(s.end()),
        begin(s.begin()),
        filename(*move(filename))
    { }

    [[noreturn, gnu::cold]]
    void error (const char* in, Str mess) {
         // Diagnose line and column number
         // I'm not sure the col is exactly right
        uint line = 1;
        const char* last_lf = begin - 1;
        for (const char* p2 = begin; p2 != in; p2++) {
            if (*p2 == '\n') {
                line++;
                last_lf = p2;
            }
        }
        uint col = in - last_lf;
        raise(e_ParseFailed, cat(
            mess, " at ", filename, ':', line, ':', col
        ));
    }

    [[gnu::cold]]
    void check_error_chars (const char* in) {
        if (*in <= ' ' || *in >= 127) {
            error(in, cat(
                "Unrecognized byte <", to_hex_digit(uint8(*in) >> 4),
                to_hex_digit(*in & 0xf), '>'
            ));
        }
        switch (*in) {
            case ANY_RESERVED_SYMBOL:
                error(in, cat("Reserved symbol ", *in));
            default: return;
        }
    }

    ///// NON-SEMANTIC CONTENT

    const char* skip_comment (const char* in) {
        in += 2;  // for two -s
        while (in < end) {
            if (*in++ == '\n') break;
        }
        return in;
    }
    NOINLINE const char* skip_ws (const char* in) {
        while (in < end) {
            switch (*in) {
                case ANY_WS: in++; break;
                case '-': {
                    if (in + 1 < end && in[1] == '-') {
                        in = skip_comment(in);
                        break;
                    }
                    else return in;
                }
                default: return in;
            }
        }
        return in;
    }
    NOINLINE const char* skip_commas (const char* in) {
        while (in < end) {
            switch (*in) {
                case ANY_WS: case ',': in++; break;
                case '-': {
                    if (in + 1 < end && in[1] == '-') {
                        in = skip_comment(in);
                        break;
                    }
                    else return in;
                }
                default: return in;
            }
        }
        return in;
    }

    ///// STRINGS

    const char* got_x_escape (const char* in, char& r) {
        {
            if (in + 2 >= end) goto invalid_x;
            int n0 = from_hex_digit(in[0]);
            if (n0 < 0) goto invalid_x;
            int n1 = from_hex_digit(in[1]);
            if (n1 < 0) goto invalid_x;
            in += 2;
            r = n0 << 4 | n1;
            return in;
        }
        invalid_x: error(in, "Invalid \\x escape sequence");
    }

    const char* got_u_escape (const char* in, UniqueString& out) {
        UniqueString16 units (Capacity(1));
         // Process multiple \uXXXX sequences at once so
         // that we can fuse UTF-16 surrogates.
        for (;;) {
            if (in + 4 >= end) goto invalid_u;
            int n0 = from_hex_digit(in[0]);
            if (n0 < 0) goto invalid_u;
            int n1 = from_hex_digit(in[1]);
            if (n1 < 0) goto invalid_u;
            int n2 = from_hex_digit(in[2]);
            if (n0 < 0) goto invalid_u;
            int n3 = from_hex_digit(in[3]);
            if (n1 < 0) goto invalid_u;
            units.push_back(n0 << 12 | n1 << 8 | n2 << 4 | n3);
            in += 4;
            if (in + 2 < end && in[0] == '\\' && in[1] == 'u') {
                in += 2;
            }
            else break;
        }
        out.append_expect_capacity(from_utf16(units));
        return in;
        invalid_u: error(in, "Invalid \\u escape sequence");
    }

    NOINLINE const char* got_string (const char* in, AnyString& r) {
        in++;  // for the "
         // Find the end of the string and determine upper bound of required
         // capacity.
        usize n_escapes = 0;
        const char* p = in;
        while (p < end) {
            switch (*p) {
                case '"': goto start;
                case '\\':
                    n_escapes++;
                    p += 2;
                    break;
                default: p++; break;
            }
        }
        error(in, "String not terminated by end of input");
        start:
         // If there aren't any escapes we can just memcpy the whole string
        if (!n_escapes) {
            new (&r) AnyString(in, p);
            return p+1; // For the "
        }
         // Otherwise preallocate
        auto out = UniqueString(Capacity(p - in - n_escapes));
         // Now read the string
        while (in < end) {
            char c = *in++;
            switch (c) {
                case '"':
                    new (&r) AnyString(move(out));
                    return in;
                case '\\': {
                    expect(in < end);
                    switch (*in++) {
                        case '"': c = '"'; break;
                        case '\\': c = '\\'; break;
                         // Dunno why this is in json
                        case '/': c = '/'; break;
                        case 'b': c = '\b'; break;
                        case 'f': c = '\f'; break;
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'x': in = got_x_escape(in, c); break;
                        case 'u':
                            in = got_u_escape(in, out);
                            continue; // Skip the push_back
                        default: in--; error(in, "Unknown escape sequence");
                    }
                    break;
                }
                default: [[likely]] break;
            }
            out.push_back_expect_capacity(c);
        }
        never();
    }

    NOINLINE const char* find_word_end (const char* in) {
        in++; // First character already known to be part of word
        while (in < end) {
            switch (*in) {
                case ANY_LETTER: case ANY_DECIMAL_DIGIT: case ANY_WORD_SYMBOL:
                    in++; break;
                case ':': {
                     // Allow :: for c++ types
                    if (in < end && in[1] == ':') {
                        in += 2;
                        break;
                    }
                    else return in;
                }
                case '"': {
                    error(in, "\" cannot occur inside a word (are you missing the first \"?)");
                }
                default: return in;
            }
        }
        return in;
    }

    ///// NUMBERS

    [[noreturn, gnu::cold]]
    void error_invalid_number (const char* in) { error(in, "Couldn't parse number"); }

    template <bool hex>
    const char* parse_floating (const char* in, Tree& r, const char* word_end, bool minus) {
        double floating;
        auto [num_end, ec] = std::from_chars(
            in, word_end, floating,
            hex ? std::chars_format::hex
                : std::chars_format::general
        );
        expect(num_end > in);
        if (num_end == word_end) {
            new (&r) Tree(minus ? -floating : floating);
            if (hex) r.flags |= PREFER_HEX;
            return num_end;
        }
        else error_invalid_number(in);
    }

    template <bool hex>
    const char* parse_number (const char* in, Tree& r, const char* word_end, bool minus) {
        if (in >= word_end) error_invalid_number(in);
        switch (in[0]) {
            case ANY_DECIMAL_DIGIT: break;
            case ANY_HEX_LETTER: if (hex) break; else error_invalid_number(in);
            case '.': error(in, "Number cannot start with .");
            default: error_invalid_number(in);
        }
        int64 integer;
        auto [num_end, ec] = std::from_chars(
            in, word_end, integer, hex ? 16 : 10
        );
        expect(num_end > in);
        if (num_end == word_end) {
            if (minus) {
                if (integer == 0) new (&r) Tree(-0.0);
                else new (&r) Tree(-integer);
            }
            else new (&r) Tree(integer);
            if (hex) r.flags |= PREFER_HEX;
            return num_end;
        }
         // Forbid . without a digit after
        else if (num_end < word_end && num_end[0] == '.') {
            if (num_end + 1 >= word_end ||
                !(hex ? std::isxdigit(num_end[1]) : std::isdigit(num_end[1]))
            ) error(in, "Number cannot end with .");
        }
        return parse_floating<hex>(in, r, word_end, minus);
    }

    NOINLINE const char* parse_number_based (const char* in, Tree& r, const char* word_end, bool minus) {
         // Detect hex prefix
        if (in + 1 < word_end && (Str(in, 2) == "0x" || Str(in, 2) == "0X")) {
            in += 2;
            return parse_number<true>(in, r, word_end, minus);
        }
        else return parse_number<false>(in, r, word_end, minus);
    }

    const char* got_plus (const char* in, Tree& r) {
        auto word_end = find_word_end(in);
        if (Str(in, word_end) == "+nan") {
            new (&r) Tree(std::numeric_limits<double>::quiet_NaN());
            return word_end;
        }
        else if (Str(in, word_end) == "+inf") {
            new (&r) Tree(std::numeric_limits<double>::infinity());
            return word_end;
        }
        return parse_number_based(in+1, r, word_end, false);
    }

    const char* got_minus (const char* in, Tree& r) {
        auto word_end = find_word_end(in);
        if (Str(in, word_end) == "-inf") {
            new (&r) Tree(-std::numeric_limits<double>::infinity());
            return word_end;
        }
        return parse_number_based(in+1, r, word_end, true);
    }

    const char* got_digit (const char* in, Tree& r) {
        return parse_number_based(in, r, find_word_end(in), false);
    }

    ///// COMPOUND

    const char* got_array (const char* in, Tree& r) {
        UniqueArray<Tree> a;
        in++;  // for the [
        while (in < end) {
            in = skip_commas(in);
            switch (*in) {
                case ':': error(in, "Cannot have : in an array");
                case ']': new (&r) Tree(move(a)); return in + 1;
                default:
                    in = parse_term(in, a.emplace_back());
                    break;
            }
        }
        error(in, "Array is not terminated");
    }

    const char* got_object (const char* in, Tree& r) {
        UniqueArray<TreePair> o;
        in++;  // for the {
        while (in < end) {
            in = skip_commas(in);
            if (in >= end) goto not_terminated;
            switch (*in) {
                case ':': error(in, "Missing key before : in object");
                case '}': new (&r) Tree(move(o)); return in + 1;
                default: break;
            }
            Tree key;
            in = parse_term(in, key);
            if (key.form != STRING) {
                error(in, "Can't use non-string as key in object");
            }
            in = skip_ws(in);
            if (in >= end) goto not_terminated;
            if (*in == ':') in++;
            else [[unlikely]] {
                check_error_chars(in);
                error(in, "Missing : after name in object");
            }
            in = skip_ws(in);
            if (in >= end) goto not_terminated;
            switch (*in) {
                case ',':
                case '}': error(in, "Missing value after : in object");
                default: {
                    Tree& value = o.emplace_back(move(key), Tree()).second;
                    in = parse_term(in, value);
                    break;
                }
            }
        }
        not_terminated: error(in, "Object is not terminated");
    }

    ///// SHORTCUTS

    NOINLINE const char* parse_shortcut_name (const char* in, AnyString& r) {
        if (in >= end) goto nope;
        switch (*in) {
            case ANY_WORD_STARTER: {
                auto word_end = find_word_end(in);
                auto word = Str(in, word_end);
                if (word == "null" || word == "true" || word == "false") {
                    goto nope;
                }
                new (&r) AnyString(StaticString(word));
                return word_end;
            }
            case '"': {
                return got_string(in, r);
            }
            default: goto nope;
        }
        nope: error(in, cat("Expected string for shortcut name after ", in[-1]));
    }

    NOINLINE const char* set_shortcut (const char* in, MoveRef<AnyString> name_, MoveRef<Tree> value_) {
        auto name = *move(name_);
        auto value = *move(value_);
        for (auto& sc : shortcuts) {
            if (sc.first == name) {
                error(in, cat("Multiple declarations of shortcut &", name));
            }
        }
        shortcuts.emplace_back(move(name), move(value));
        return in;
    }

    const char* get_shortcut (const char* in, Tree& r, Str name) {
        for (auto& sc : shortcuts) {
            if (sc.first == name) {
                new (&r) Tree(sc.second);
                return in;
            }
        }
        error(in, cat("Unknown shortcut *", name));
    }

    const char* got_decl (const char* in, Tree& r) {
        in++;  // for the &
        AnyString name;
        in = parse_shortcut_name(in, name);
        in = skip_ws(in);
        if (in < end && *in == ':') {
            in++;
            in = skip_ws(in);
            Tree value;
            in = parse_term(in, value);
            in = set_shortcut(in, move(name), move(value));
            in = skip_commas(in);
            return parse_term(in, r);
        }
        else {
            in = parse_term(in, r);
            return set_shortcut(in, move(name), r);
        }
    }

    const char* got_shortcut (const char* in, Tree& r) {
        in++;  // for the *
        AnyString name;
        in = parse_shortcut_name(in, name);
        return get_shortcut(in, r, name);
    }

    ///// TERM

    const char* parse_term (const char* in, Tree& r) {
        if (in >= end) error(in, "Expected term but ran into end of file");
        switch (*in) {
            case ANY_WORD_STARTER: {
                auto word_end = find_word_end(in);
                auto word = Str(in, word_end);
                if (word == "null") new (&r) Tree(null);
                else if (word == "true") new (&r) Tree(true);
                else if (word == "false") new (&r) Tree(false);
                else new (&r) Tree(word);
                return word_end;
            }

            case ANY_DECIMAL_DIGIT: return got_digit(in, r);
            case '+': return got_plus(in, r);
             // Comments starting with -- should already have been skipped by a
             // previous skip_ws().
            case '-': return got_minus(in, r);
            case '.': error(in, "Number cannot begin with .");

            case '"': {
                AnyString str;
                in = got_string(in, str);
                new (&r) Tree(move(str));
                return in;
            }
            case '[': return got_array(in, r);
            case '{': return got_object(in, r);

            case '&': return got_decl(in, r);
            case '*': return got_shortcut(in, r);
            default:
                check_error_chars(in);
                error(in, cat("Unexpected ", *in));
        }
    }

    ///// TOP

    Tree parse () {
        const char* in = begin;
         // Skip BOM
        if (in + 2 < end && Str(in, 3) == "\xef\xbb\xbf") {
            in += 3;
        }
        in = skip_ws(in);
        Tree r;
        in = parse_term(in, r);
        in = skip_ws(in);
        if (in != end) error(in, "Extra stuff at end of document");
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
