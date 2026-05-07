#include "print.h"

#include <cstring>
#include <charconv>

#include "../../uni/buffers.h"
#include "../../uni/io.h"
#include "char-props.private.h"

namespace ayu {
namespace in {

using O = PrintOptions;

struct Printer {
     // Put less-frequently-accessed data here.
    char* end;
    PrintOptions opts;
    u32 indent;
    char* begin;

    Printer (PrintOptions f) : end(null), opts(f), indent(0), begin(null) { }

    ~Printer () {
        if (begin) SharableBuffer<char>::deallocate(begin);
    }

     // We are going with a continual-reallocation strategy.  I tried doing an
     // estimate-first-and-allocate-once strategy, but when the length
     // estimation gets complicated enough, it gets slower than reallocating.
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
        return pstr_reserved(p, s);
    }
    static
    char* pstr_reserved (char* p, Str s) {
        return s.size() + (char*)std::memcpy(p, s.data(), s.size());
    }

    NOINLINE static
    char* print_undefined (Printer&, char* p, const Tree&) {
        return pstr_reserved(p, "!(undefined)");
    }

    NOINLINE static
    char* print_null (Printer&, char* p, const Tree&) {
        return pstr_reserved(p, "null");
    }

    NOINLINE static
    char* print_bool (Printer&, char* p, const Tree& t) {
        bool b = t.data.as_bool;
        u32 v = *(u32*)(b ? "true" : "fals");
        std::memcpy(p, &v, 4);
        p[4] = 'e';
        return p + 5 - b;
    }

    char* print_index (char* p, u32 v) {
        p = reserve(p, 15);
        *p++ = ' '; *p++ = ' '; *p++ = '-'; *p++ = '-'; *p++ = ' ';
        return write_decimal_digits(p, count_decimal_digits(v), v);
    }

    char* print_small_int (char* p, const Tree& t) {
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
            p = write_hex_digits(
                p, count_hex_digits(v), v
            );
            return p;
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
        double v = t.data.as_double;
        if (!std::isfinite(v)) {
            if (opts % O::Json) {
                if (v > 0) return pstr_reserved(p, "1e999");
                else if (v < 0) return pstr_reserved(p, "-1e999");
                else return pstr_reserved(p, "null");
            }
            else {
                u32 repr = *(u32*)(
                    v > 0 ? "+inf" : v < 0 ? "-inf" : "+nan"
                );
                return 4+(char*)std::memcpy(p, &repr, 4);
            }
        }
        if (v == 0) {
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

    NOINLINE static
    char* print_number (Printer& self, char* p, const Tree& t) {
        if (t.floaty) return self.print_double(p, t);
        else if (t.data.as_i64 >= 0 && t.data.as_i64 < 10) {
            return self.print_small_int(p, t);
        }
        else return self.print_i64(p, t);
    }

    NOINLINE static
    char* print_string (Printer& self, char* p, const Tree& t) {
        expect(t.form == Form::String);
        auto s = Str(t);
        return self.print_string_s(p, s, &t);
    }

    char* print_string_s (char* p, Str s, const Tree* t) {
        p = reserve(p, 2 + s.size());
        if (opts % O::Json) goto quoted;
        if (s == "" || s == "null" || s == "true" || s == "false") goto quoted;
        if (s[0] == '.') {
            if (s.size() > 1 && char_illegal_after_dot(s[1])) {
                goto quoted;
            }
        }
        else if (char_term(s[0]) != CHAR_TERM_WORD) {
            goto quoted;
        }

        for (auto sp = s.begin() + 1; sp != s.end(); sp++) {
            if (sp[0] == ':') {
                if (sp + 1 != s.end() && sp[1] == ':') {
                    sp++;
                    continue;
                }
                else goto quoted;
            }
            else if (!char_continues_word(sp[0])) goto quoted;
        }
         // No need to quote
        return pstr_reserved(p, s);
         // Yes need to quote
        quoted:
         // The expanded form of a string uses raw newlines and tabs instead of
         // escaping them.  Ironically, this takes fewer characters than the
         // compact form, so expand it when not pretty-printing.
        bool expand = opts % O::Json ? false
                    : !(opts % O::Pretty) ? true
                    : !t ? false
                    : t->flags % TreeFlags::PreferExpanded ? true
                    : t->flags % TreeFlags::PreferCompact ? false
                    : t->size > 50;
        *p++ = '"';
        for (u32 i = 0; i < s.size(); i++) {
            if (!char_needs_escape(s[i])) [[likely]] {
                *p++ = s[i];
            }
             // Don't quite have enough bits in the char prop table to
             // differentiate these.  Fortunately \n and \t are one apart so
             // they can be checked with one comparison.
            else if ((s[i] == '\n' || s[i] == '\t') && expand) {
                *p++ = s[i];
            }
            else {
                 // +6 for \u00xx, +1 for ", -1 for original char
                p = reserve(p, 6 + s.size() - i);
                char repl = char_escape_table[u8(s[i])];
                if (repl) {
                    *p++ = '\\'; *p++ = repl;
                }
                else {
                    if (opts % O::Json) {
                        *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    }
                    else {
                        *p++ = '\\'; *p++ = 'x';
                    }
                    *p++ = to_hex_digit(s[i] >> 4);
                    *p++ = to_hex_digit(s[i] & 0xf);
                }
            }
        }
        *p++ = '"';
        return p;
    }

    char* print_newline (char* p) {
        p = reserve(p, 1 + indent * 4);
        *p++ = '\n';
        for (u32 ind = indent; ind; ind--) {
            p = pstr_reserved(p, "    ");
        }
        return p;
    }

    NOINLINE static
    char* print_array (Printer& self, char* p, const Tree& t) {
        expect(t.form == Form::Array);
        auto a = Slice<Tree>(t);
        if (a.empty()) {
            return pstr_reserved(p, "[]");
        }

         // Print "small" arrays compactly.
        bool expand = !(self.opts % O::Pretty) ? false
                    : t.flags % TreeFlags::PreferExpanded ? true
                    : t.flags % TreeFlags::PreferCompact ? false
                    : a.size() > 8;

        bool show_indices = expand
                         && a.size() > 2
                         && !(self.opts % O::Json);
        *p++ = '[';
        if (expand) {
            self.indent += 1;
            for (auto& elem : a) {
                if (self.opts % O::Json && &elem != &a.front()) {
                    p = self.pchar(p, ',');
                }
                p = self.print_newline(p);
                p = self.print_tree(p, elem);
                if (show_indices) {
                    p = self.print_index(p, &elem - &a.front());
                }
            }
            self.indent -= 1;
            p = self.print_newline(p);
        }
        else {
            for (auto& elem : a) {
                p = self.reserve(p, 25);
                if (&elem != &a.front()) {
                    *p++ = self.opts % O::Json ? ',' : ' ';
                }
                p = self.print_tree_reserved(p, elem);
            }
        }
        return self.pchar(p, ']');
    }

    NOINLINE static
    char* print_object (Printer& self, char* p, const Tree& t) {
        expect(t.form == Form::Object);
        auto o = Slice<TreePair>(t);
        if (o.empty()) {
            return pstr_reserved(p, "{}");
        }

         // If both prefer_expanded and prefer_compact are set, I think the one
         // who set prefer_expanded is more likely to have a good reason.
        bool expand = !(self.opts % O::Pretty) ? false
                    : t.flags % TreeFlags::PreferExpanded ? true
                    : t.flags % TreeFlags::PreferCompact ? false
                    : o.size() > 1;

        *p++ = '{';
        if (expand) {
            self.indent += 1;
            for (auto& attr : o) {
                if (self.opts % O::Json && &attr != &o.front()) {
                    p = self.pchar(p, ',');
                }
                p = self.print_newline(p);
                p = self.print_string_s(p, attr.first, null);
                p = self.reserve(p, 26);
                *p++ = ':'; *p++ = ' ';
                p = self.print_tree_reserved(p, attr.second);
            }
            self.indent -= 1;
            p = self.print_newline(p);
        }
        else {
            for (auto& attr : o) {
                if (&attr != &o.front()) {
                    p = self.pchar(p, self.opts % O::Json ? ',' : ' ');
                }
                p = self.print_string_s(p, attr.first, null);
                p = self.reserve(p, 25);
                *p++ = ':';
                p = self.print_tree_reserved(p, attr.second);
            }
        }
        return self.pchar(p, '}');
    }

    NOINLINE static
    char* print_error (Printer& self, char* p, const Tree& t) {
        try {
            std::rethrow_exception(std::exception_ptr(t));
        }
        catch (const std::exception& e) {
            const char* what = e.what();
            usize len = std::strlen(what);
            p = self.reserve(p, 3 + len);
            *p++ = '!'; *p++ = '(';
            p = len+(char*)std::memcpy(p, what, len);
            *p++ = ')';
            return p;
        }
    }

    char* print_tree (char* p, const Tree& t) {
         // The caller is guaranteed to have a stack frame, but some of the
         // functions we could call wouldn't need one if they didn't have to
         // call reserve, so reserve the maximum needed for an atomic item.
        p = reserve(p, 24);
        return print_tree_reserved(p, t);
    }
    char* print_tree_reserved (char* p, const Tree& t) {
        static constexpr decltype(&print_null) printers [8] = {
            &print_undefined,
            &print_null,
            &print_bool,
            &print_number,
            &print_string,
            &print_array,
            &print_object,
            &print_error
        };
        expect(u8(t.form) < 8);
        return printers[u8(t.form)](*this, p, t);
    }

    UniqueString print (const Tree& t, u32 cap) {
        expect(cap >= 24);
        begin = SharableBuffer<char>::allocate_plenty(cap);
        end = begin + SharableBuffer<char>::header(begin)->capacity;
         // Do it
        char* p = print_tree_reserved(begin, t);
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
    u32 cap = t.form == Form::String ? 32 + t.size : 32 + 32 * t.size;
    return printer.print(t, cap);
}

UniqueString tree_to_string_for_file (const Tree& t, PrintOptions opts) {
    validate_print_options(opts);
    if (!(opts % O::Compact)) opts |= O::Pretty;
    Printer printer (opts);
    return printer.print(t, 4080 - 16);
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

    auto pretty = string_from_file(resource_filepath(IRI("ayu-test:/print-pretty.ayu")));
    auto compact = string_from_file(resource_filepath(IRI("ayu-test:/print-compact.ayu")));
    auto pretty_json = string_from_file(resource_filepath(IRI("ayu-test:/print-pretty.json")));
    auto compact_json = string_from_file(resource_filepath(IRI("ayu-test:/print-compact.json")));
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
