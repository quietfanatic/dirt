#include "print.h"

#include <cstring>
#include <charconv>

#include "../../uni/buffers.h"
#include "../../uni/io.h"
#include "char-cases.private.h"

namespace ayu {
namespace in {

using O = PrintOptions;

struct Printer {
    char* end;
    PrintOptions opts;
    char* begin;

    Printer (PrintOptions f) : end(null), opts(f), begin(null) { }

    ~Printer () {
        if (begin) SharableBuffer<char>::deallocate(begin);
    }

     // We are going with a continual-reallocation strategy.  I tried doing an
     // estimate-first-and-allocate-once strategy, but once the length
     // estimation gets complicated enough, it ends up slower than reallocating.
    NOINLINE
    char* extend (char* p, u32 more) {
        char* old_begin = begin;
        char* new_begin = SharableBuffer<char>::allocate_plenty(
            p - old_begin + more
        );
        begin = (char*)std::memcpy(
            new_begin, old_begin, p - old_begin
        );
        end = begin + SharableBuffer<char>::header(begin)->capacity;
        SharableBuffer<char>::deallocate(old_begin);
        return p - old_begin + begin;
    }

    char* reserve (char* p, u32 more) {
        if (p + more >= end) [[unlikely]] return extend(p, more);
        else return p;
    }

    char* pchar (char* p, char c) {
        p = reserve(p, 1);
        *p = c;
        return p+1;
    }

    char* pstr (char* p, Str s) {
        p = reserve(p, s.size());
        std::memcpy(p, s.data(), s.size());
        return p + s.size();
    }

    NOINLINE
    char* print_null (char* p) {
        return pstr(p, "null");
    }

    NOINLINE
    char* print_bool (char* p, const Tree& t) {
        return t.data.as_bool ? pstr(p, "true") : pstr(p, "false");
    }

    char* print_index (char* p, u32 v) {
        p = reserve(p, 15);
        *p++ = ' '; *p++ = ' '; *p++ = '-'; *p++ = '-'; *p++ = ' ';
        return write_decimal_digits(p, count_decimal_digits(v), v);
    }

    NOINLINE
    char* print_small_int (char* p, const Tree& t) {
        p = reserve(p, 3);
        i64 v = t.data.as_i64;
        expect(v >= 0 && v < 10);
        bool hex = !(opts % O::Json) && t.flags % TreeFlags::PreferHex;
        if (hex) {
            *p++ = '0'; *p++ = 'x';
        }
        *p++ = '0' + v;
        return p;
    }

    NOINLINE
    char* print_i64 (char* p, const Tree& t) {
        p = reserve(p, 20);
        i64 v = t.data.as_i64;
        expect(v < 0 || v >= 10);
        if (v < 0) {
            *p++ = '-';
            v = -v;
        }
        bool hex = !(opts % O::Json) && t.flags % TreeFlags::PreferHex;
        if (hex) {
             // std::to_chars is bulky for decimal, but it's better than I can
             // program for hexadecimal.
            *p++ = '0'; *p++ = 'x';
            auto [ptr, ec] = std::to_chars(
                p, p+16, u64(v), 16
            );
            expect(ec == std::errc());
            return ptr;
        }
        else {
            p = write_decimal_digits(
                p, count_decimal_digits(v), v
            );
            return p;
        }
    }

    NOINLINE
    char* print_double (char* p, const Tree& t) {
        p = reserve(p, 24);
        double v = t.data.as_double;
        if (v != v) {
            if (opts % O::Json) {
                return 4+(char*)std::memcpy(p, "null", 4);
            }
            else {
                return 4+(char*)std::memcpy(p, "+nan", 4);
            }
        }
        else if (v == +inf) {
            if (opts % O::Json) {
                return 5+(char*)std::memcpy(p, "1e999", 5);
            }
            else {
                return 4+(char*)std::memcpy(p, "+inf", 4);
            }
        }
        else if (v == -inf) {
            if (opts % O::Json) {
                return 6+(char*)std::memcpy(p, "-1e999", 6);
            }
            else {
                return 4+(char*)std::memcpy(p, "-inf", 4);
            }
        }
        else if (v == 0) {
            if (1.0/v == -inf) {
                *p++ = '-';
            }
            *p++ = '0';
            return p;
        }

        bool hex = !(opts % O::Json) && t.flags % TreeFlags::PreferHex;
        if (hex) {
            if (v < 0) {
                *p++ = '-';
                v = -v;
            }
            *p++ = '0';
            *p++ = 'x';
        }
         // Not even gonna try beating the stdlib's floating point to_chars.
        auto [ptr, ec] = std::to_chars(
            p, p+24, v, hex
                ? std::chars_format::hex
                : std::chars_format::general
        );
        expect(ec == std::errc());
        return ptr;
    }

    NOINLINE
    char* print_quoted_expanded (char* p, Str s) {
        p = reserve(p, 2 + s.size());
        *p++ = '"';
        for (u32 i = 0; i < s.size(); i++) {
            char esc;
            switch (s[i]) {
                case '"': esc = '"'; goto escape;
                case '\\': esc = '\\'; goto escape;
                case '\b': esc = 'b'; goto escape;
                case '\f': esc = 'f'; goto escape;
                case '\r': esc = 'r'; goto escape;
                default: {
                    if (u8(s[i]) < u8(' ')) [[unlikely]] {
                        if (opts % O::Json) {
                            p = reserve(p, 6 + s.size() - i);
                            *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                            *p++ = to_hex_digit(s[i] >> 4);
                            *p++ = to_hex_digit(s[i] & 0xf);
                        }
                        else {
                            p = reserve(p, 4 + s.size() - i);
                            *p++ = '\\'; *p++ = 'x';
                            *p++ = to_hex_digit(s[i] >> 4);
                            *p++ = to_hex_digit(s[i] & 0xf);
                        }
                    }
                    else *p++ = s[i];
                    continue;
                }
            }
            escape:
             // +1 for \, +1 for final "
            p = reserve(p, 2 + s.size() - i);
            *p++ = '\\'; *p++ = esc;
        }
        *p++ = '"';
        return p;
    }

    NOINLINE
    char* print_quoted_contracted (char* p, Str s) {
        p = reserve(p, 2 + s.size());
        *p++ = '"';
        for (u32 i = 0; i < s.size(); i++) {
            char esc;
            switch (s[i]) {
                case '"': esc = '"'; goto escape;
                case '\\': esc = '\\'; goto escape;
                case '\b': esc = 'b'; goto escape;
                case '\f': esc = 'f'; goto escape;
                case '\n': esc = 'n'; goto escape;
                case '\r': esc = 'r'; goto escape;
                case '\t': esc = 't'; goto escape;
                default: {
                    if (u8(s[i]) < u8(' ')) [[unlikely]] {
                        if (opts % O::Json) {
                            p = reserve(p, 6 + s.size() - i);
                            *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                            *p++ = to_hex_digit(s[i] >> 4);
                            *p++ = to_hex_digit(s[i] & 0xf);
                        }
                        else {
                            p = reserve(p, 4 + s.size() - i);
                            *p++ = '\\'; *p++ = 'x';
                            *p++ = to_hex_digit(s[i] >> 4);
                            *p++ = to_hex_digit(s[i] & 0xf);
                        }
                    }
                    else *p++ = s[i];
                    continue;
                }
            }
            escape:
             // +1 for \, +1 for final "
            p = reserve(p, 2 + s.size() - i);
            *p++ = '\\'; *p++ = esc;
        }
        *p++ = '"';
        return p;
    }

    char* print_string (char* p, Str s, const Tree* t) {
        if (opts % O::Json) {
            return print_quoted_contracted(p, s);
        }
        else return print_string_nojson(p, s, t);
    }

    NOINLINE
    char* print_string_nojson (char* p, Str s, const Tree* t) {
        if (s == "") return pstr(p, "\"\"");
        if (s == "null") return pstr(p, "\"null\"");
        if (s == "true") return pstr(p, "\"true\"");
        if (s == "false") return pstr(p, "\"false\"");

        switch (s[0]) {
            case ANY_WORD_STARTER: break;
            case '.': {
                if (s.size() > 1) switch (s[1]) {
                    case ANY_DECIMAL_DIGIT: case '-': case '+': goto quoted;
                    default: break;
                }
                break;
            }
            default: goto quoted;
        }

        for (auto sp = s.begin() + 1; sp != s.end(); sp++)
        switch (sp[0]) {
            case ':': {
                if (sp + 1 != s.end() && sp[1] == ':') {
                    sp++;
                    continue;
                }
                else goto quoted;
            }
            case ANY_LETTER: case ANY_DECIMAL_DIGIT:
            case ANY_WORD_SYMBOL: continue;
            default: goto quoted;
        }
         // No need to quote
        return pstr(p, s);
         // Yes need to quote
        quoted:
         // The expanded form of a string uses raw newlines and tabs instead of
         // escaping them.  Ironically, this takes fewer characters than the
         // compact form, so expand it when not pretty-printing.
        bool expand = !(opts % O::Pretty) ? true
                    : !t ? false
                    : t->flags % TreeFlags::PreferExpanded ? true
                    : t->flags % TreeFlags::PreferCompact ? false
                    : t->size > 50;
        if (expand) {
            return print_quoted_expanded(p, s);
        } else {
            return print_quoted_contracted(p, s);
        }
    }

    char* print_newline (char* p, u32 ind) {
        p = reserve(p, 1 + ind * 4);
        *p++ = '\n';
        for (; ind; ind--) p = 4+(char*)std::memcpy(p, "    ", 4);
        return p;
    }

    NOINLINE
    char* print_array (char* p, const Tree& t, u32 ind) {
        expect(t.form == Form::Array);
        auto a = Slice<Tree>(t);
        if (a.empty()) {
            return pstr(p, "[]");
        }

         // Print "small" arrays compactly.
        bool expand = !(opts % O::Pretty) ? false
                    : t.flags % TreeFlags::PreferExpanded ? true
                    : t.flags % TreeFlags::PreferCompact ? false
                    : a.size() > 8;

        bool show_indices = expand
                         && a.size() > 2
                         && !(opts % O::Json);
        p = pchar(p, '[');
        if (expand) {
            for (auto& elem : a) {
                if (opts % O::Json && &elem != &a.front()) {
                    p = pchar(p, ',');
                }
                p = print_newline(p, ind + 1);
                p = print_tree(p, elem, ind + 1);
                if (show_indices) {
                    p = print_index(p, &elem - &a.front());
                }
            }
            p = print_newline(p, ind);
        }
        else {
            for (auto& elem : a) {
                if (&elem != &a.front()) {
                    p = pchar(p, opts % O::Json ? ',' : ' ');
                }
                p = print_tree(p, elem, ind);
            }
        }
        return pchar(p, ']');
    }

    NOINLINE
    char* print_object (char* p, const Tree& t, u32 ind) {
        expect(t.form == Form::Object);
        auto o = Slice<TreePair>(t);
        if (o.empty()) {
            return pstr(p, "{}");
        }

         // If both prefer_expanded and prefer_compact are set, I think the one
         // who set prefer_expanded is more likely to have a good reason.
        bool expand = !(opts % O::Pretty) ? false
                    : t.flags % TreeFlags::PreferExpanded ? true
                    : t.flags % TreeFlags::PreferCompact ? false
                    : o.size() > 1;

        p = pchar(p, '{');
        if (expand) {
            for (auto& attr : o) {
                if (opts % O::Json && &attr != &o.front()) {
                    p = pchar(p, ',');
                }
                p = print_newline(p, ind + 1);
                p = print_string(p, attr.first, null);
                p = pstr(p, ": ");
                p = print_tree(p, attr.second, ind + 1);
            }
            p = print_newline(p, ind);
        }
        else {
            for (auto& attr : o) {
                if (&attr != &o.front()) {
                    p = pchar(p, opts % O::Json ? ',' : ' ');
                }
                p = print_string(p, attr.first, null);
                p = pchar(p, ':');
                p = print_tree(p, attr.second, ind);
            }
        }
        return pchar(p, '}');
    }

    NOINLINE
    char* print_error (char* p, const Tree& t) {
        try {
            std::rethrow_exception(std::exception_ptr(t));
        }
        catch (const std::exception& e) {
            const char* what = e.what();
            usize len = std::strlen(what);
            p = reserve(p, 3 + len);
            *p++ = '!'; *p++ = '(';
            p = len+(char*)std::memcpy(p, what, len);
            *p++ = ')';
            return p;
        }
    }

     // Anything that prints a string should be NOINLINEd here, because printing
     // a string requires a possible non-tail-call to extend(), which requires
     // saving things on the stack.  If everything is NOINLINE, this function
     // will require no prologue or epilogue.
    NOINLINE
    char* print_tree (char* p, const Tree& t, u32 ind) {
        switch (t.form) {
            case Form::Null: return print_null(p);
            case Form::Bool: return print_bool(p, t);
            case Form::Number: {
                if (t.floaty) return print_double(p, t);
                else if (t.data.as_i64 >= 0 && t.data.as_i64 < 10) {
                    return print_small_int(p, t);
                }
                else return print_i64(p, t);
            }
            case Form::String: return print_string(p, Str(t), &t);
            case Form::Array: return print_array(p, t, ind);
            case Form::Object: return print_object(p, t, ind);
            case Form::Error: return print_error(p, t);
            default: never();
        }
    }

    UniqueString print (const Tree& t, u32 cap) {
        begin = SharableBuffer<char>::allocate_plenty(cap);
        end = begin + SharableBuffer<char>::header(begin)->capacity;
         // Do it
        char* p = print_tree(begin, t, 0);
        if (opts % O::Pretty) p = pchar(p, '\n');
         // Make return
        UniqueString r;
        r.impl.size = p - begin;
        r.impl.data = begin;
        begin = null;
        return r;
    }
};

static void validate_print_options (PrintOptions opts) {
    if (opts % ~O::ValidBits ||
        (opts % O::Pretty && opts % O::Compact)
    ) {
        raise(e_PrintOptionsInvalid, "Further info NYI");
    }
}

} using namespace in;

UniqueString tree_to_string (const Tree& t, PrintOptions opts) {
    validate_print_options(opts);
    if (!(opts % O::Pretty)) opts |= O::Compact;
    Printer printer (opts);
    u32 cap = t.form == Form::Array || t.form == Form::Object
        ? 32 * t.size : 32;
    return printer.print(t, cap);
}

UniqueString tree_to_string_for_file (const Tree& t, PrintOptions opts) {
    validate_print_options(opts);
    if (!(opts % O::Compact)) opts |= O::Pretty;
    Printer printer (opts);
     // Lilac can't quite fast-allocate a whole 4k page
    return printer.print(t, 4064);
}

void tree_to_file (const Tree& t, AnyString filename, PrintOptions opts) {
    return string_to_file(tree_to_string_for_file(t, opts), move(filename));
}

} using namespace ayu;

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"
#include "../resources/resource.h"
#include "../test/test-environment.private.h"
#include "parse.h"

static tap::TestSet tests ("dirt/ayu/data/print", []{
    using namespace tap;

    test::TestEnvironment env;

    auto pretty = string_from_file(resource_filename(IRI("ayu-test:/print-pretty.ayu")));
    auto compact = string_from_file(resource_filename(IRI("ayu-test:/print-compact.ayu")));
    auto pretty_json = string_from_file(resource_filename(IRI("ayu-test:/print-pretty.json")));
    auto compact_json = string_from_file(resource_filename(IRI("ayu-test:/print-compact.json")));
     // Remove final LF
    compact.pop_back();
    compact_json.pop_back();

    Tree t = tree_from_string(pretty);

    auto test = [](Str got, Str expected, Str name){
        if (!is(got, expected, name)) {
            u32 i = 0;
            for (; i < got.size() && i < expected.size(); i++) {
                if (got[i] != expected[i]) {
                    diag(cat("First difference at ",
                        i, " |", got[i], '|', expected[i], '|'
                    ));
                    return;
                }
            }
            if (got.size() != expected.size()) {
                diag(cat("Size difference got ",
                    got.size(), " expected ", expected.size()
                ));
            }
        }
    };
    test(tree_to_string(t, O::Pretty), pretty, "Pretty");
    test(tree_to_string(t, O::Compact), compact, "Compact");
    test(tree_to_string(t, O::Pretty|O::Json), pretty_json, "Pretty Json");
    test(tree_to_string(t, O::Compact|O::Json), compact_json, "Compact Json");
    test(tree_to_string(Tree(1.0)), "1", "Autointification small");
    test(tree_to_string(Tree(145.0)), "145", "Autointification large");

    done_testing();
});
#endif
