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

struct Macro {
    Tree name;
    Tree value;
    u16 depth;
};

 // Parsing is simple enough that we don't need a separate lexer step.
struct Parser {

     // Limit how many nested arrays and objects we have.  If you have that much
     // data in a structured text format, you're going to have performance
     // problems anyway, and you should offload some of it to binary or flat
     // text formats.
    static constexpr u16 max_depth = 200;

///// TOP

    const char* end;
    const char* begin;
    u16 depth;
    u16 depth_watermark;
    bool in_macro_name = false;
    bool in_macro_value = false;

    Parser (Str s) :
        end(s.end()),
        begin(s.begin())
    { }

    Tree parse () {
        depth_watermark = depth = 0;
        const char* in = begin;
        in = skip_bom(in);
        in = skip_ws(in);
        Tree r;
        in = parse_item(in, r);
        in = skip_ws(in);
        if (in != end) error(in, "Extra stuff at end of document");
        assume(depth == 0);
        return r;
    }

    UniqueArray<Tree> parse_list () {
        depth_watermark = depth = 1;
        UniqueArray<Tree> r;
        const char* in = begin;
        in = skip_bom(in);
        in = skip_ws(in);
        while (in != end) {
            Tree e;
            in = parse_item(in, e);
            r.push_back(move(e));
            in = skip_comma(in);
        }
        assume(depth == 1);
        return r;
    }

    void inc_depth (const char* in) {
        if (++depth > max_depth) {
            error(in, "Exceeded limit of 200 nested arrays/objects");
        }
        if (depth > depth_watermark) depth_watermark = depth;
    }
    void dec_depth () {
        --depth;
    }

///// TERM

    NOINLINE const char* parse_item (const char* in, Tree& r) {
         // Table has to be inside member function to see functions declared
         // below it.
        static constexpr decltype(&got_word) table [] = {
            &got_error,
            &got_word,
            &got_digit,
            &got_plus,
            &got_minus,
            &got_dot,
            &got_string,
            &got_array,
            &got_object,
            &got_macro
        };
        if (in >= end) error(in, "Expected item but ran into end of input");
        auto index = char_item(*in);
        assume(u32(index) < sizeof(table) / sizeof(table[0]));
        return table[u32(index)](*this, in, r);
    }

///// WORDS (unquoted)

    NOINLINE Str parse_word (const char* in) {
        const char* start = in;
      next:
        in++; // First character already known to be part of word
      check:
        if (in < end) {
            if (char_continues_word(*in)) [[likely]] goto next;
            else if (*in == ':') {
                 // Allow :: for c++ types
                if (in + 2 < end && in[1] == ':'
                 && char_item(in[2]) == CHAR_ITEM_WORD
                ) {
                    in += 3;
                    goto check;
                }
            }
            else if (*in == '"') {
                error(in, "\" cannot occur inside a word");
            }
            else [[likely]];
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
        error(word.begin(), "Couldn't parse number");
    }

    const char* parse_floating (Str word, Tree& r, bool minus) {
        double floating;
        const char* word_end = word.end();
        auto [num_end, ec] = std::from_chars(
            word.begin(), word_end, floating
        );
        if (num_end == word_end) {
            new (&r) Tree(minus ? -floating : floating);
            return num_end;
        }
        else error_invalid_number(word);
    }

    const char* parse_decimal (Str word, Tree& r, bool minus) {
        auto [num_end, integer] = read_decimal_digits<u64>(word.begin(), word.end());
        if (num_end == word.begin()) {
            error_invalid_number(word);
        }
        if (num_end == word.end()) {
            if (minus) {
                if (integer == 0) new (&r) Tree(-0.0);
                else new (&r) Tree(-integer);
            }
            else new (&r) Tree(integer);
            return num_end;
        }
         // Forbid ending with a .
        if (num_end[0] == '.') {
            if (num_end + 1 >= word.end() ||
                (num_end[1] | ('a' & ~'A')) == 'e'
            ) error(num_end, "Number cannot end with a dot.");
        }
        return parse_floating(word, r, minus);
    }

    const char* parse_hexadecimal (Str word, Tree& r, bool minus) {
         // Using an unsigned integer parser will reject words that start with a
         // + or -.
        auto [num_end, integer] = read_hex_digits<u64>(word.begin(), word.end());
        if (num_end != word.end()) {
            error_invalid_number(word);
        }
        if (minus) {
            if (integer == 0) new (&r) Tree(-0.0);
            else new (&r) Tree(-integer);
        }
        else new (&r) Tree(integer);
        r.flags |= TreeFlags::PreferHex;
        return word.end();
    }

    NOINLINE const char* parse_number_based (Str word, Tree& r, bool minus) {
         // Detect hex prefix
        if (word.size() >= 2 && (word.chop(2) == "0x" || word.chop(2) == "0X")) {
            return parse_hexadecimal(word.slice(2), r, minus);
        }
        else return parse_decimal(word, r, minus);
    }

    NOINLINE static
    const char* got_digit (Parser& self, const char* in, Tree& r) {
        return self.parse_number_based(self.parse_word(in), r, false);
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
        else return self.parse_number_based(word.slice(1), r, false);
    }

    NOINLINE static
    const char* got_minus (Parser& self, const char* in, Tree& r) {
         // Comments should already have been recognized by this point.
        auto word = self.parse_word(in);
        if (word == "-inf") {
            new (&r) Tree(-std::numeric_limits<double>::infinity());
            return word.end();
        }
        else return self.parse_number_based(word.slice(1), r, true);
    }

    NOINLINE static
    const char* got_dot (Parser& self, const char* in, Tree& r) {
        auto word = self.parse_word(in);
        if (word.size() > 1 && word[1] >= '0' && word[1] <= '9') {
            self.error(in, "Number cannot start with a dot");
        }
        else {
            new (&r) Tree(word);
            return word.end();
        }
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
        auto out = UniqueString(Capacity(assume(p - in - extra_input)));
         // Now read the string
        assume(in < p);
        while (in < p) {
            char c = *in++;
            if (c == '\\') [[unlikely]] {
                assume(in < p);
                c = *in++;
                if (u8(c) <= ' ' || u8(c) >= char_escape_table.size()) {
                    if (c == 'x') {
                        c = self.got_x_escape(in);
                        in += 2;
                        goto push;
                    }
                    else if (c == 'u') {
                        in = self.got_u_escape(in, out);
                        continue;
                    }
                }
                else if (char repl = char_escape_table[u8(c)]) {
                    c = repl;
                    goto push;
                }
                self.error(in-1, "Unknown escape sequence");
            }
            push: out.push_back_assume_capacity(c);
        }
        assume(*in++ == '"');
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
        out.append_assume_capacity(from_utf16(units));
        return in;
        invalid_u: error(in, "Invalid \\u escape sequence");
    }

///// COMPOUND

    NOINLINE static
    const char* got_array (Parser& self, const char* in, Tree& r) {
        self.inc_depth(in);
        in++;  // for the [
        in = self.skip_ws(in);
        UniqueArray<Tree> a;
        while (in < self.end) {
            if (*in == ']') {
                new (&r) Tree(move(a));
                self.dec_depth();
                return in + 1;
            }
            in = self.parse_item(in, a.emplace_back());
            in = self.skip_comma(in);
        }
        self.error(in, "Missing ]");
    }

    NOINLINE static
    const char* got_object (Parser& self, const char* in, Tree& r) {
        self.inc_depth(in);
        in++;  // for the {
        in = self.skip_ws(in);
        UniqueArray<TreePair> o;
        while (in < self.end) {
            if (*in == '}') {
                new (&r) Tree(move(o));
                self.dec_depth();
                return in + 1;
            }
            Tree key;
            in = self.parse_item(in, key);
            if (key.form != Form::String) {
                self.error(in, "Key in object is not string");
            }
            in = self.skip_ws(in);
            if (in >= self.end) break;
            if (*in == ':') in++;
            else [[unlikely]] {
                self.error(in, "Missing : after key in object");
            }
            in = self.skip_ws(in);
            if (in >= self.end) break;
            assume(key.form == Form::String);
            Tree& value = o.emplace_back(SharedString(move(key)), Tree()).second;
            in = self.parse_item(in, value);
            in = self.skip_comma(in);
        }
        self.error(in, "Missing }");
    }

///// MACROS

     // std::unordered_map is supposedly slow, so we'll use an array instead.
     // We'll rethink if we ever need to parse a document with a large amount of
     // macros (I can't imagine for my use cases having more than 20 or so).
    UniqueArray<Macro> macros;

    NOINLINE static
    const char* got_macro (Parser& self, const char* in, Tree& r) {
        if (self.in_macro_name) {
            self.error(in, "Cannot use macro in name of macro");
        }
        in++; // For the (
        in = self.skip_ws(in);
        { // Reduce scope of name to allow tail call
            Tree name;
            self.in_macro_name = true;
            in = self.parse_item(in, name);
            self.in_macro_name = false;
            if (name.form != Form::String) {
                self.error(in, "Macro name is not string");
            }
            in = self.skip_ws(in);
            if (in < self.end && *in == ')') {
                in++;
                 // Invocation
                 // Search macros backwards to allow overriding
                for (auto m = self.macros.rbegin(); m != self.macros.rend(); m++) {
                    assume(name.form == Form::String);
                    assume(m->name.form == Form::String);
                    if (Str(m->name) == Str(name)) {
                        if (self.depth + m->depth > max_depth) {
                            self.error(in, "Exceeded limit of 200 nested arrays/objects after expanding macro");
                        }
                        new (&r) Tree(m->value);
                        return in;
                    }
                }
                assume(name.form == Form::String);
                self.error(in, cat("Undefined macro ", Str(name)));
            }
            else if (in < self.end && *in == ':') {
                 // Definition
                if (self.in_macro_name) {
                    self.error(in, "Cannot define macro in name of macro");
                }
                if (self.in_macro_value) {
                    self.error(in, "Cannot define macro while defining macro");
                }
                in++;
                in = self.skip_ws(in);
                {
                    u16 old_watermark = self.depth_watermark;
                    Tree value;
                    self.in_macro_value = true;
                    in = self.parse_item(in, value);
                    self.in_macro_value = false;
                    self.macros.emplace_back(
                        move(name), move(value), self.depth_watermark - self.depth
                    );
                    self.depth_watermark = old_watermark;
                }
                in = self.skip_ws(in);
                if (in >= self.end || *in != ')') {
                    self.error(in, "Expected )");
                }
                in++;
            }
            else {
                self.error(in, "Expected ) or :");
            }
        }
        in = self.skip_ws(in);
        return self.parse_item(in, r);
    }

///// NON-SEMANTIC CONTENT

    template <bool comma> NOINLINE static
    const char* skip_ws_comma (const char* in, const char* end) {
        while (in < end) {
            if (char_is_ws(*in)) [[likely]] {
                in++;
                continue;
            }
            else if (*in == '-') [[unlikely]] {
                if (in + 1 < end && in[1] == '-') {
                    in += 2;
                     // Unrolling/vectorizing this just isn't worth it.
                    #pragma GCC unroll 0
                    #pragma GCC novector
                    while (in < end) { if (*in++ == '\n') break; }
                }
                else break;
            }
            else if (comma && *in == ',') {
                in++;
                return skip_ws_comma<false>(in, end);
            }
            else break;
        }
        return in;
    }

    const char* skip_ws (const char* in) {
        return skip_ws_comma<false>(in, end);
    }

    const char* skip_comma (const char* in) {
        return skip_ws_comma<true>(in, end);
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
        self.error(in, cat("Expected item"));
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
    n(".0e4");
    y(".+4", Tree(".+4"));
    y(".-4", Tree(".-4"));
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
    y("(foo:1) ( foo )", Tree(1));
    y("( foo : 1 ) (foo)", Tree(1));
    y("(foo:foo) (foo)", Tree("foo"));
    n("(null:4) (null");
    y("(\"null\":4) (\"null\")", Tree(4));
    n("(5:40) (5)");
    y("(\"5\":40) (\"5\")", Tree(40));
    y("[(foo:1) (foo)]", Tree::array(Tree(1)));
    y("{(key:asdf) (key):(key)}", Tree::object(TreePair{"asdf", Tree("asdf")}));
    y("3 --4", Tree(3));
    y("#", Tree("#"));
    y("#foo", Tree("#foo"));
    y("{$borp:$borp}", Tree::object(TreePair{"$borp", Tree("$borp")}));
    n("{borp:44 (borp):(borp)}");
    n("(foo:)");
    n("(foo:1)");
    n("(foo)");
    n("4 (foo:4)");
    n("(foo:(foo)) 1");
    y("(foo:2) (foo:[(foo) (foo)]) (foo)", Tree::array(Tree(2), Tree(2)));
    n("((a):1) (a)");
    n("(a:(b:b) a) (a)");
    n("(a:(b:b) a) (b)");
    n("[+nana]");
     // Test depth limit
    auto big = UniqueString(Capacity(402));
    for (u32 i = 0; i < 201; i++) {
        big.push_back_assume_capacity('[');
    }
    for (u32 i = 0; i < 201; i++) {
        big.push_back_assume_capacity(']');
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
