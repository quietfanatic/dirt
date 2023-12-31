#include "print.h"

#include <cstring>
#include <charconv>

#include "../../uni/buffers.h"
#include "../../uni/io.h"
#include "char-cases.private.h"

namespace ayu {
namespace in {

struct Printer {
    char* end;
    PrintOptions opts;
    char* begin;

    Printer (PrintOptions f) : end(null), opts(f), begin(null) { }

    ~Printer () {
        if (begin) SharableBuffer<char>::deallocate(begin);
    }

    NOINLINE
    char* extend (char* p, usize more = 1) {
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

    char* pchar (char* p, char c) {
        if (p == end) [[unlikely]] p = extend(p);
        *p = c;
        return p+1;
    }
    char* pstr (char* p, Str s) {
        if (p + s.size() > end) [[unlikely]] p = extend(p, s.size());
        std::memcpy(p, s.data(), s.size());
        return p + s.size();
    }

    char* print_uint64 (char* p, uint64 v, bool hex) {
        if (v == 0) {
            return pchar(p, '0');
        }
        if (end - p < 20) [[unlikely]] p = extend(p, 20);
        auto [ptr, ec] = std::to_chars(
            p, p+20, v, hex ? 16 : 10
        );
        expect(ec == std::errc());
        return ptr;
    }

    char* print_int64 (char* p, int64 v, bool hex) {
        if (v == 0) {
            return pchar(p, '0');
        }
        if (v < 0) {
            p = pchar(p, '-');
        }
        if (hex) {
            p = pstr(p, "0x");
        }
        return print_uint64(p, v < 0 ? -v : v, hex);
    }

    char* print_double (char* p, double v, bool hex) {
        if (hex) {
            if (v < 0) {
                p = pchar(p, '-');
                v = -v;
            }
            p = pstr(p, "0x");
        }
        if (end - p < 24) [[unlikely]] p = extend(p, 24);
        auto [ptr, ec] = std::to_chars(
            p, p+24, v, hex
                ? std::chars_format::hex
                : std::chars_format::general
        );
        expect(ec == std::errc());
        return ptr;
    }

    char* print_quoted (char* p, Str s, bool expand) {
        p = pchar(p, '"');
        for (auto c : s)
        switch (c) {
            case '"': p = pstr(p, "\\\""); break;
            case '\\': p = pstr(p, "\\\\"); break;
            case '\b': p = pstr(p, "\\b"); break;
            case '\f': p = pstr(p, "\\f"); break;
            case '\n':
                if (expand) p = pchar(p, c);
                else p = pstr(p, "\\n");
                break;
            case '\r': p = pstr(p, "\\r"); break;
            case '\t':
                if (expand) p = pchar(p, c);
                else p = pstr(p, "\\t");
                break;
            default: p = pchar(p, c); break;
        }
        return pchar(p, '"');
    }

    char* print_string (char* p, Str s, bool expand) {
        if (opts & JSON) {
            return print_quoted(p, s, false);
        }
        if (s == "") return pstr(p, "\"\"");
        if (s == "//") return pstr(p, "\"//\"");
        if (s == "null") return pstr(p, "\"null\"");
        if (s == "true") return pstr(p, "\"true\"");
        if (s == "false") return pstr(p, "\"false\"");

        switch (s[0]) {
            case ANY_WORD_STARTER: break;
            default: return print_quoted(p, s, expand);
        }

        for (auto sp = s.begin(); sp != s.end(); sp++)
        switch (sp[0]) {
            case ':': {
                if (sp + 1 != s.end() && sp[1] == ':') {
                    sp++;
                    continue;
                }
                else return print_quoted(p, s, expand);
            }
            case ANY_LETTER: case ANY_DECIMAL_DIGIT:
            case ANY_WORD_SYMBOL: continue;
            default: return print_quoted(p, s, expand);
        }
         // No need to quote
        return pstr(p, s);
    }

    char* print_newline (char* p, uint n) {
        p = pchar(p, '\n');
        for (; n; n--) p = pstr(p, "    ");
        return p;
    }

    static usize approx_width (TreeRef t) {
        switch (t->form) {
            case Form::String: return t->meta >> 1;
            case Form::Array: {
                usize r = 2;
                for (auto& e : Slice<Tree>(*t)) {
                    r += 1 + approx_width(e);
                }
                return r;
            }
            case Form::Object: {
                usize r = 2;
                for (auto& [key, value] : Slice<TreePair>(*t)) {
                    r += 2 + key.size() + approx_width(value);
                }
                return r;
            }
            default: return 4;
        }
    }

    char* print_tree (char* p, TreeRef t, uint ind) {
        switch (t->form) {
            case Form::Null: return pstr(p, "null");
            case Form::Bool: {
                auto s = t->data.as_bool ? Str("true") : Str("false");
                return pstr(p, s);
            }
            case Form::Number: {
                if (t->meta) {
                    double v = t->data.as_double;
                    if (v != v) {
                        return pstr(p, opts & JSON ? "null" : "+nan");
                    }
                    else if (v == +inf) {
                        return pstr(p, opts & JSON ? Str("1e999") : Str("+inf"));
                    }
                    else if (v == -inf) {
                        return pstr(p, opts & JSON ? Str("-1e999") : Str("-inf"));
                    }
                    else if (v == 0) {
                        if (1.0/v == -inf) {
                            p = pchar(p, '-');
                        }
                        return pchar(p, '0');
                    }
                    else {
                        bool hex = !(opts & JSON) && t->flags & TreeFlags::PreferHex;
                        return print_double(p, v, hex);
                    }
                }
                else {
                    bool hex = !(opts & JSON) && t->flags & TreeFlags::PreferHex;
                    return print_int64(p, t->data.as_int64, hex);
                }
            }
            case Form::String: {
                 // The expanded form of a string uses raw newlines and tabs
                 // instead of escaping them.  Ironically, this takes fewer
                 // characters than the compact form, so expand it when not
                 // pretty-printing.
                bool expand = !(opts & PRETTY) ? true
                            : t->flags & TreeFlags::PreferExpanded ? true
                            : t->flags & TreeFlags::PreferCompact ? false
                            : t->meta >> 1 > 50;
                return print_string(p, Str(*t), expand);
            }
            case Form::Array: {
                auto a = Slice<Tree>(*t);
                if (a.empty()) {
                    return pstr(p, "[]");
                }

                 // Print "small" arrays compactly.
                bool expand = !(opts & PRETTY) ? false
                            : t->flags & TreeFlags::PreferExpanded ? true
                            : t->flags & TreeFlags::PreferCompact ? false
                            : a.size() <= 2 ? false
                            : approx_width(t) > 50;

                bool show_indices = expand
                                 && a.size() > 2
                                 && !(opts & JSON);
                p = pchar(p, '[');
                for (auto& elem : a) {
                    if (&elem == &a.front()) {
                        if (expand) p = print_newline(p, ind + 1);
                    }
                    else {
                        if (expand) {
                            if (opts & JSON) p = pchar(p, ',');
                            p = print_newline(p, ind + 1);
                        }
                        else {
                            p = pchar(p, opts & JSON ? ',' : ' ');
                        }
                    }
                    p = print_tree(p, elem, ind + expand);
                    if (show_indices) {
                        p = pstr(p, "  -- ");
                        p = print_uint64(p, &elem - &a.front(), false);
                    }
                }
                if (expand) p = print_newline(p, ind);
                return pchar(p, ']');
            }
            case Form::Object: {
                auto o = Slice<TreePair>(*t);
                if (o.empty()) {
                    return pstr(p, "{}");
                }

                 // TODO: Decide what to do if both PREFER flags are set
                bool expand = !(opts & PRETTY) ? false
                            : t->flags & TreeFlags::PreferExpanded ? true
                            : t->flags & TreeFlags::PreferCompact ? false
                            : o.size() <= 1 ? false
                            : approx_width(t) > 50;

                p = pchar(p, '{');
                for (auto& attr : o) {
                    if (&attr == &o.front()) {
                        if (expand) {
                            p = print_newline(p, ind + 1);
                        }
                    }
                    else {
                        if (expand) {
                            if (opts & JSON) p = pchar(p, ',');
                            p = print_newline(p, ind + 1);
                        }
                        else {
                            p = pchar(p, opts & JSON ? ',' : ' ');
                        }
                    }
                    p = print_string(p, attr.first, false);
                    p = pchar(p, ':');
                    if (expand) p = pchar(p, ' ');
                    p = print_tree(p, attr.second, ind + expand);
                }
                if (expand) p = print_newline(p, ind);
                return pchar(p, '}');
            }
            case Form::Error: {
                try {
                    std::rethrow_exception(std::exception_ptr(*t));
                }
                catch (const std::exception& e) {
                     // TODO: change to !
                    p = pstr(p, "?(");
                    p = pstr(p, e.what());
                    p = pchar(p, ')');
                    return p;
                }
            }
            default: never();
        }
    }

    UniqueString print (TreeRef t) {
        usize capacity = 256;
        begin = SharableBuffer<char>::allocate_exact(capacity);
        end = begin + capacity;
         // Do it
        char* p = print_tree(begin, t, 0);
        if (opts & PRETTY) p = pchar(p, '\n');
         // Make return
        UniqueString r;
        r.impl.size = p - begin;
        r.impl.data = begin;
         // Don't waste too much space
        if (r.impl.size < (end - begin) / 2) r.shrink_to_fit();
        begin = null;
        return r;
    }
};

static void validate_print_options (PrintOptions opts) {
    if (opts & ~VALID_PRINT_OPTION_BITS ||
        ((opts & PRETTY) && (opts & COMPACT))
    ) {
        raise(e_PrintOptionsInvalid, "Further info NYI");
    }
}

} using namespace in;

UniqueString tree_to_string (TreeRef t, PrintOptions opts) {
    validate_print_options(opts);
    if (!(opts & PRETTY)) opts |= COMPACT;
    Printer printer (opts);
    return printer.print(t);
}

void tree_to_file (TreeRef t, AnyString filename, PrintOptions opts) {
    validate_print_options(opts);
    if (!(opts & COMPACT)) opts |= PRETTY;
    auto output = Printer(opts).print(t);
    string_to_file(output, move(filename));
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

    auto pretty = string_from_file(resource_filename("ayu-test:/print-pretty.ayu"));
    auto compact = string_from_file(resource_filename("ayu-test:/print-compact.ayu"));
    auto pretty_json = string_from_file(resource_filename("ayu-test:/print-pretty.json"));
    auto compact_json = string_from_file(resource_filename("ayu-test:/print-compact.json"));
     // Remove final LF
    compact.pop_back();
    compact_json.pop_back();

    Tree t = tree_from_string(pretty);

    auto test = [](Str got, Str expected, std::string name){
        if (!is(got, expected, name)) {
            usize i = 0;
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
    test(tree_to_string(t, PRETTY), pretty, "Pretty");
    test(tree_to_string(t, COMPACT), compact, "Compact");
    test(tree_to_string(t, PRETTY|JSON), pretty_json, "Pretty JSON");
    test(tree_to_string(t, COMPACT|JSON), compact_json, "Compact JSON");
    test(tree_to_string(Tree(1.0)), "1", "Autointification small");
    test(tree_to_string(Tree(145.0)), "145", "Autointification small");

    done_testing();
});
#endif
