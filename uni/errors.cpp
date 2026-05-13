#include "errors.h"

#include "io.h"

namespace uni {
using namespace in;

NOINLINE
Error::Error (ErrorCode c, SharedString&& d) noexcept :
    code(c), details(move(d))
{ }
NOINLINE
Error::Error (std::exception_ptr&& e) noexcept :
    code(e_External), details("(nonstandard exception)"), external(move(e))
{
    try { std::rethrow_exception(external); }
    catch (std::exception& e) { details = e.what(); }
    catch (...) { }
}
NOINLINE Error::Error (const Error&) noexcept = default;
NOINLINE Error::~Error () { }

const char* Error::what () const noexcept {
    if (!what_cache) {
        StaticString code_s = code;
        usize len = code_s.size() + 2 + details.size();
        for (usize i = 0; i < tags.size(); i++) {
            len += 5 + tags[i].first.size() + 2 + tags[i].second.size();
        }
        what_cache = UniqueString(Capacity(len));
        what_cache.append_expect_capacity(code_s);
        what_cache.append_expect_capacity("; ");
        what_cache.append_expect_capacity(details);
        for (usize i = 0; i < tags.size(); i++) {
            what_cache.append_expect_capacity("\n    ");
            what_cache.append_expect_capacity(tags[i].first);
            what_cache.append_expect_capacity(": ");
            what_cache.append_expect_capacity(tags[i].second);
        }
    }
    return what_cache.c_str();
}

NOINLINE
Str Error::get_tag (Str name) noexcept {
    for (auto& [n, v] : tags) {
        if (n == name) return v;
    }
    return "";
}
NOINLINE
void in::set_tag_impl (Error& e, SharedString::Impl name, SharedString::Impl value) noexcept {
    SharedString n; n.impl = name;
    SharedString v; v.impl = value;
    e.what_cache = "";
    for (auto it = e.tags.rbegin(); it != e.tags.rend(); it++) {
        if (it->first == n) {
            it->second = move(v);
            return;
        }
    }
    e.tags.emplace_back(move(n), move(v));
}
NOINLINE
void in::add_tag_impl (Error& e, SharedString::Impl name, SharedString::Impl value) noexcept {
    SharedString n; n.impl = name;
    SharedString v; v.impl = value;
    e.what_cache = "";
    e.tags.emplace_back(move(n), move(v));
}
NOINLINE
void in::rethrow_with_tag_impl (Error& e, SharedString::Impl name, SharedString::Impl value) {
    set_tag_impl(e, name, value);
#ifndef NDEBUG
    expect(&current_error() == &e);
#endif
    throw;
}
NOINLINE
void Error::rethrow_with_tag (StaticString name, const char* value) {
    set_tag(name, value);
#ifndef NDEBUG
    expect(&current_error() == this);
#endif
    throw;
}

Error& current_error () noexcept {
    try { throw; } catch (Error& e) { return e; }
    catch (...) {
        try { throw Error(std::current_exception()); }
        catch (Error& e) { return e; }
    }
}

void in::raise_impl (ErrorCode code, SharedString::Impl details) {
    SharedString d; d.impl = details;
    throw Error(code, move(d));
}

void unrecoverable_exception (Str when) noexcept {
    Error& e = current_error();
    warn_utf8(cat(
        "ERROR: Unrecoverable exception ", when, '\n',
        e.what()
    ));
    std::terminate();
}

} // uni
