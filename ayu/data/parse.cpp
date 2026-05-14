#include "parse.h"

#include <cstring>
#include <charconv>
#include <limits>

#include "../../uni/io.h"
#include "../../uni/text.h"
#include "../../uni/utf.h"
#include "../data/tree.h"
#include "char-props.private.h"

namespace ayu {

namespace in {

struct SourcePos {
    u32 line;
    u32 col;
};

 // Parsing is simple enough that we don't need a separate lexer step.
struct Parser {

     // Limit how many nested arrays and objects we have.  If you have that much
     // data in a structured text format, you're going to have performance
     // problems anyway, and you should offload some of it to binary or flat
     // text formats.
    static constexpr u32 max_depth = 200;

///// TOP

    const char* end;
    const char* begin;
    u32 shallowth;

    Parser (Str s) :
        end(s.end()),
        begin(s.begin())
    { }

    Tree parse () {
        shallowth = max_depth + 1;
        const char* in = begin;
        in = skip_bom(in);
        in = skip_ws(in);
        Tree r;
        in = parse_term(in, r);
        in = skip_ws(in);
        if (in != end) error(in, "Extra stuff at end of document");
        expect(shallowth == max_depth + 1);
        return r;
    }

    UniqueArray<Tree> parse_list () {
        UniqueArray<Tree> r;
        shallowth = max_depth;
        const char* in = begin;
        in = skip_bom(in);
        in = skip_ws(in);
        while (in != end) {
            Tree e;
            in = parse_term(in, e);
            r.push_back(move(e));
            in = skip_comma(in);
        }
        expect(shallowth == max_depth);
        return r;
    }

///// TERM

    NOINLINE const char* parse_term (const char* in, Tree& r) {
         // Table has to be inside member function to see functions declared
         // below it.
        static constexpr decltype(&got_word) table [] = {
            &got_error,
            &got_word,
            &got_digit,
            &got_dot,
            &got_plus,
            &got_minus,
            &got_string,
            &got_array,
            &got_object,
            &got_shortcut
        };
        if (in >= end) error(in, "Expected term but ran into end of input");
        auto index = char_term(*in);
        expect(u32(index) < sizeof(table) / sizeof(table[0]));
        return table[u32(index)](*this, in, r);
    }

///// WORDS (unquoted)

    NOINLINE Str parse_word (const char* in) {
        const char* start = in;
        in++; // First character already known to be part of word
        while (in < end) {
            if (char_continues_word(*in)) [[likely]] {
                in++;
            }
            else if (*in == ':') {
                 // Allow :: for c++ types
                if (in + 1 < end && in[1] == ':') {
                    in += 2;
                }
                else return Str(start, in);
            }
            else if (*in == '"') {
                error(in, "\" cannot occur inside a word (is the first \" missing?)");
            }
            else [[likely]] return Str(start, in);
        }
        return Str(start, in);
    }

    NOINLINE static
    const char* got_word (Parser& self, const char* in, Tree& r) {
        auto word = self.parse_word(in);
        if (word == "null") new (&r) Tree(null);
        else if (word == "true") new (&r) Tree(true);
        else if (word == "false") new (&r) Tree(false);
        else new (&r) Tree(word);
        return word.end();
    }

///// NUMBERS

    [[noreturn, gnu::cold]] NOINLINE
    void error_invalid_number (Str word) {
        if (word.end() < end) {
            check_error_chars(word.end());
        }
        error(word.begin(), "Couldn't parse number");
    }

    template <bool hex>
    const char* parse_floating (Str word, Tree& r, bool minus) {
        double floating;
        const char* word_end = word.end();
        auto [num_end, ec] = std::from_chars(
            word.begin(), word_end, floating,
            hex ? std::chars_format::hex
                : std::chars_format::general
        );
        if (num_end == word_end) {
            TreeFlags f = hex ? TreeFlags::PreferHex : TreeFlags();
            new (&r) Tree(minus ? -floating : floating, f);
            return num_end;
        }
        else error_invalid_number(word);
    }

    template <bool hex>
    const char* parse_number (Str word, Tree& r, bool minus) {
         // Using an unsigned integer parser will reject words that start with a
         // + or -.
        auto read = hex ? read_hex_digits<u64> : read_decimal_digits<u64>;
        auto [num_end, integer] = read(word.begin(), word.end());
        if (num_end == word.begin()) {
            error_invalid_number(word);
        }
        if (num_end == word.end()) {
            TreeFlags f = hex ? TreeFlags::PreferHex : TreeFlags();
            if (minus) {
                if (integer == 0) new (&r) Tree(-0.0, f);
                else new (&r) Tree(-integer, f);
            }
            else new (&r) Tree(integer, f);
            return num_end;
        }
         // Forbid ending with a .
        if (num_end[0] == '.') {
            if (num_end + 1 >= word.end() ||
                (num_end[1] & ~('a' & ~'A')) == (hex ? 'P' : 'E')
            ) error(num_end, "Number cannot end with a dot.");
        }
        return parse_floating<hex>(word, r, minus);
    }

    NOINLINE const char* parse_number_based (Str word, Tree& r, bool minus) {
         // Detect hex prefix
        if (word.size() >= 2 && (word.chop(2) == "0x" || word.chop(2) == "0X")) {
            return parse_number<true>(word.slice(2), r, minus);
        }
        else return parse_number<false>(word, r, minus);
    }

    NOINLINE static
    const char* got_digit (Parser& self, const char* in, Tree& r) {
        return self.parse_number_based(self.parse_word(in), r, false);
    }

    NOINLINE static
    const char* got_dot (Parser& self, const char* in, Tree& r) {
        auto word = self.parse_word(in);
        if (word.size() > 1 && char_illegal_after_dot(word[1])) {
            self.error(in, "Number cannot start with a dot.");
        }
        new (&r) Tree(word);
        return word.end();
    }

    NOINLINE static
    const char* got_plus (Parser& self, const char* in, Tree& r) {
        auto word = self.parse_word(in);
        if (word == "+nan") {
            new (&r) Tree(std::numeric_limits<double>::quiet_NaN());
            return word.end();
        }
        else if (word == "+inf") {
            new (&r) Tree(std::numeric_limits<double>::infinity());
            return word.end();
        }
        return self.parse_number_based(word.slice(1), r, false);
    }

    NOINLINE static
    const char* got_minus (Parser& self, const char* in, Tree& r) {
         // Comments should already have been recognized by this point.
        auto word = self.parse_word(in);
        if (word == "-inf") {
            new (&r) Tree(-std::numeric_limits<double>::infinity());
            return word.end();
        }
        return self.parse_number_based(word.slice(1), r, true);
    }

///// STRINGS (quoted)

    NOINLINE static
    const char* got_string (Parser& self, const char* in, Tree& r) {
        in++;  // for the "
         // Find the end of the string and determine upper bound of required
         // capacity.
        u32 extra_input = 0;
        const char* p;
        for (p = in; p < self.end; p++) {
            if (*p == '"') goto start;
            else if (*p == '\\') [[unlikely]] {
                if (p >= self.end) goto unterminated;
                 // We could get away with always adding 1, but then a
                 // string composed entirely of \x would be overallocated by
                 // a factor of 3.  A UTF-16 \u can emit at most 3 UTF-8
                 // bytes, so this is the right amount for it as well.
                p++;
                extra_input += (*p >= 'u' && *p <= 'x') ? 3 : 1;
            }
        }
        unterminated:
        self.error(in, "Missing \" before end of input");
        start:
         // If there aren't any escapes we can just memcpy the whole string
        if (!extra_input) {
            new (&r) Tree(UniqueString(in, p));
            return p+1; // For the "
        }
         // Otherwise preallocate
        auto out = UniqueString(Capacity(expect(p - in - extra_input)));
         // Now read the string
        expect(in < p);
        while (in < p) {
            char c = *in++;
            if (c == '\\') [[unlikely]] {
                expect(in < p);
                c = *in++;
                if (u8(c) <= ' ' || u8(c) >= char_escape_table.size()) {
                    goto invalid_escape;
                }
                else if (char repl = char_escape_table[u8(c)]) {
                    c = repl;
                    goto push;
                }
                else if (c == 'x') {
                    c = self.got_x_escape(in);
                    in += 2;
                    goto push;
                }
                else if (c == 'u') {
                    in = self.got_u_escape(in, out);
                    continue;
                }
                invalid_escape: self.error(in-1, "Unknown escape sequence");
            }
            push: out.push_back_expect_capacity(c);
        }
        expect(*in++ == '"');
        new (&r) Tree(move(out));
        return in;
    }

    char got_x_escape (const char* in) {
        {
            if (in + 2 > end) goto invalid_x;
            int n0 = from_hex_digit(in[0]);
            if (n0 < 0) goto invalid_x;
            int n1 = from_hex_digit(in[1]);
            if (n1 < 0) goto invalid_x;
            return n0 << 4 | n1;
        }
        invalid_x: error(in, "Invalid \\x escape sequence");
    }

     // NOINLINE this because it's complicated and we only have it for JSON
     // compatibility.
    NOINLINE
    const char* got_u_escape (const char* in, UniqueString& out) {
        UniqueString16 units (Capacity(1));
         // Process multiple \uXXXX sequences at once so
         // that we can fuse UTF-16 surrogates.
        loop: {
            if (in + 4 > end) goto invalid_u;
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
            if (in + 2 <= end && in[0] == '\\' && in[1] == 'u') {
                in += 2;
                goto loop;
            }
        }
        out.append_expect_capacity(from_utf16(units));
        return in;
        invalid_u: error(in, "Invalid \\u escape sequence");
    }

///// COMPOUND

    NOINLINE static
    const char* got_array (Parser& self, const char* in, Tree& r) {
        if (!--self.shallowth) self.error(in, "Exceeded limit of 200 nested arrays/objects");
        const char* start = in;
        in++;  // for the [
        in = self.skip_ws(in);
        UniqueArray<Tree> a;
        while (in < self.end) {
            if (*in == '}') [[unlikely]] {
                auto sp = self.get_source_pos(start);
                self.error(in, cat(
                    "Mismatch between [ at ", sp.line, ':', sp.col, " and }"
                ));
            }
            if (*in == ']') {
                new (&r) Tree(move(a));
                ++self.shallowth;
                return in + 1;
            }
            in = self.parse_term(in, a.emplace_back());
            in = self.skip_comma(in);
        }
        self.error(in, "Missing ] before end of input");
    }

    NOINLINE static
    const char* got_object (Parser& self, const char* in, Tree& r) {
        if (!--self.shallowth) self.error(in, "Exceeded limit of 200 nested arrays/objects");
        const char* start = in;
        in++;  // for the {
        in = self.skip_ws(in);
        UniqueArray<TreePair> o;
        while (in < self.end) {
            if (*in == ']') [[unlikely]] {
                auto sp = self.get_source_pos(start);
                self.error(in, cat(
                    "Mismatch between { at ", sp.line, ':', sp.col, " and ]"
                ));
            }
            if (*in == '}') {
                new (&r) Tree(move(o));
                ++self.shallowth;
                return in + 1;
            }
            Tree key;
            in = self.parse_term(in, key);
            if (key.form != Form::String) {
                self.error(in, "Can't use non-string as key in object");
            }
            in = self.skip_ws(in);
            if (in >= self.end) goto not_terminated;
            if (*in == ':') in++;
            else [[unlikely]] {
                self.check_error_chars(in);
                self.error(in, "Missing : after name in object");
            }
            in = self.skip_ws(in);
            if (in >= self.end) goto not_terminated;
            Tree& value = o.emplace_back(SharedString(move(key)), Tree()).second;
            in = self.parse_term(in, value);
            in = self.skip_comma(in);
        }
        not_terminated: self.error(in, "Missing } before end of input");
    }

///// SHORTCUTS

     // std::unordered_map is supposedly slow, so we'll use an array instead.
     // We'll rethink if we ever need to parse a document with a large amount
     // of shortcuts (I can't imagine for my use cases having more than 20
     // or so).
    UniqueArray<TreePair> shortcuts;

    NOINLINE static
    const char* got_shortcut (Parser& self, const char* in, Tree& r) {
        Str name = self.parse_word(in);
        in += name.size();
        in = self.skip_ws(in);
        if (in < self.end && *in == '=') {
             // Declaration
            in++;
            in = self.skip_ws(in);
            {
                Tree value;
                in = self.parse_term(in, value);
                for (auto& sc : self.shortcuts) {
                    if (sc.first == name) {
                        sc.second = move(value);
                        goto found;
                    }
                }
                self.shortcuts.emplace_back(name, move(value));
            } // Destroy value so we can tail call
            found:
            in = self.skip_comma(in);
            return self.parse_term(in, r);
        }
        else {
             // Reference
            for (auto& sc : self.shortcuts) {
                if (sc.first == name) {
                    new (&r) Tree(sc.second);
                    return in;
                }
            }
            self.error(in, cat("Unknown shortcut ", name));
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

    const char* skip_ws_inline (const char* in) {
        while (in < end) {
            if (char_is_ws(*in)) {
                in++;
            }
            else if (*in == '-') [[unlikely]] {
                if (in + 1 < end && in[1] == '-') {
                    in = skip_comment(in);
                }
                else return in;
            }
            else return in;
        }
        return in;
    }

    NOINLINE const char* skip_ws (const char* in) {
        return skip_ws_inline(in);
    }

    NOINLINE const char* skip_comma (const char* in) {
        in = skip_ws_inline(in);
        if (in < end && *in == ',') {
            in = skip_ws_inline(in + 1);
        }
        return in;
    }

    const char* skip_bom (const char* in) {
        if (in + 2 < end && Str(in, 3) == "\xef\xbb\xbf") {
            in += 3;
        }
        return in;
    }

///// ERRORS

    [[gnu::cold]] NOINLINE
    SourcePos get_source_pos (const char* p) {
         // Diagnose line and column number
         // I'm not sure the col is exactly right
        u32 line = 1;
        const char* last_lf = begin - 1;
        for (const char* p2 = begin; p2 != p; p2++) {
            if (*p2 == '\n') {
                line++;
                last_lf = p2;
            }
        }
        u32 col = p - last_lf;
        return {line, col};
    };

    [[gnu::cold]] NOINLINE static
    const char* got_error (Parser& self, const char* in, Tree&) {
        self.check_error_chars(in);
        self.error(in, cat("Expected term but got ", *in));
    }

    [[gnu::cold]] NOINLINE
    void check_error_chars (const char* in) {
        if (*in < ' ' || *in >= 127) {
            error(in, cat(
                "Unrecognized byte <", to_hex_digit(u8(*in) >> 4),
                to_hex_digit(*in & 0xf), '>'
            ));
        }
        if (char_reserved(*in)) {
            error(in, cat("Reserved symbol ", *in));
        }
    }

    [[noreturn, gnu::cold]] NOINLINE
    void error (const char* in, Str mess) {
        auto pos = get_source_pos(in);
        raise(e_ParseFailed, cat(
            mess, " at ", pos.line, ':', pos.col
        ));
    }
};

} using namespace in;

 // Finally:
Tree tree_from_string (Str s) {
    require(s.size() <= SharedString::max_size_);
    return Parser(s).parse();
}

UniqueArray<Tree> tree_list_from_string (Str s) {
    require(s.size() <= SharedString::max_size_);
    return Parser(s).parse_list();
}

Tree tree_from_file (const char* path) {
    UniqueString s = string_from_file(path);
    try { return tree_from_string(s); }
    catch (Error& e) { e.rethrow_with_tag("uni::FilePath", path); }
}
Tree tree_from_file (Str path) {
    return with_c_str(path, [](auto buf){ return tree_from_file(buf); });
}

UniqueArray<Tree> tree_list_from_file (const char* path) {
    UniqueString s = string_from_file(path);
    try { return tree_list_from_string(s); }
    catch (Error& e) { e.rethrow_with_tag("uni::FilePath", path); }
}
UniqueArray<Tree> tree_list_from_file (Str path) {
    return with_c_str(path, [](auto buf){ return tree_list_from_file(buf); });
}

} using namespace ayu;

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"
#include "print.h"

static tap::TestSet tests ("dirt/ayu/data/parse", []{
    using namespace tap;
    auto y = [](StaticString s, const Tree& t){
        try_is([&]{return tree_from_string(s);}, t, cat("yes: ", s));
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
    y("../foo", Tree("../foo"));
    y("\"null\"", Tree("null"));
    y("\"true\"", Tree("true"));
    y("\"false\"", Tree("false"));
    y("\"asdf\\x33asdf\"", Tree("asdf3asdf"));
    n("\"af\\x3wasdf\"");
    n("\"asdfasdf\\x");
    y("\"asdf\\u0037asdf\"", Tree("asdf7asdf"));
    y("\"asdf\\uD83C\\uDF31asdf\"", Tree("asdf🌱asdf"));
    y("[]", Tree::array());
    n("[,]");
    n("[,,,,,]");
    y("[0 1 foo]", Tree::array(Tree(0), Tree(1), Tree("foo")));
    y("{}", Tree::object());
    n("{,}");
    y("{\"asdf\":\"foo\"}", Tree::object(TreePair{"asdf", Tree("foo")}));
    y("{\"asdf\":0}", Tree::object(TreePair{"asdf", Tree(0)}));
    y("{asdf:0}", Tree::object(TreePair{"asdf", Tree(0)}));
    n("{0:0}");
    y("{a:0 \"null\":1 \"0\":foo}",
        Tree::object(
            TreePair{"a", Tree(0)},
            TreePair{"null", Tree(1)},
            TreePair{"0", Tree("foo")}
        )
    );
    y("[[0 1] [[2] [3 4]]]",
        Tree::array(
            Tree::array(Tree(0), Tree(1)),
            Tree::array(
                Tree::array(Tree(2)),
                Tree::array(Tree(3), Tree(4))
            )
        )
    );
    y("[0,1,]", Tree::array(Tree(0), Tree(1)));
    n("[0,,1,]");
    n("[0,1,,]");
    todo(1, "in-place assignment NYI");
    y("($foo=1)", Tree(1));
    y("$foo=1 $foo", Tree(1));
    n("$\"null\"=4 $\"null\"");
    y("$null=4 $null", Tree(4));
    y("$5=40 $5", Tree(40));
    todo(1, "in-place assignment NYI");
    y("[($foo=1) $foo]", Tree::array(Tree(1), Tree(1)));
    y("[$foo=1 $foo]", Tree::array(Tree(1)));
    todo(1, "in-place assignment NYI");
    y("{($key=asdf):$key}", Tree::object(TreePair{"asdf", Tree("asdf")}));
    y("{$borp=\"bump\" $borp:$borp}", Tree::object(TreePair{"bump", Tree("bump")}));
    y("3 --4", Tree(3));
    y("#", Tree("#"));
    y("#foo", Tree("#foo"));
    n("{$borp:44 $borp:$borp}");
    n("$foo=");
    n("$foo=1");
    n("$foo");
    n("4 $foo=4");
    n("$foo=$foo");
    n("$foo=$foo 1");
    n("$$a=1 $a");
    y("$$a=1 $$a", Tree(1));
    n("$ a=1 $a");
    n("[+nana]");
     // Test depth limit
    auto big = UniqueString(Capacity(402));
    for (u32 i = 0; i < 201; i++) {
        big.push_back_expect_capacity('[');
    }
    for (u32 i = 0; i < 201; i++) {
        big.push_back_expect_capacity(']');
    }
    n(StaticString(big));
    auto redwood = Tree::array();
    for (u32 i = 0; i < 199; i++) {
        redwood = Tree::array(redwood);
    }
    y(StaticString(big.slice(1, 401)), redwood);
    done_testing();
});
#endif
