// Implementation details for tap.h

namespace tap {

namespace in {

inline
uni::UniqueArray<const TestSet*>& testers () {
    static uni::UniqueArray<const TestSet*> testers;
    return testers;
}

 // Global state
inline unsigned num_planned = 0;
inline unsigned num_tested = 0;
inline unsigned num_to_todo = 0;
inline bool block_todo = false;
inline uni::AnyString todo_excuse;
inline void(* print )(uni::Str) = [](uni::Str s){
    std::fwrite(s.data(), 1, s.size(), stdout);
};

 // Internal helpers

template <class F>
bool fail_on_throw (F code, uni::Str name) {
    try {
        return std::forward<F>(code)();
    }
    catch (const scary_exception&) { throw; }
    catch (const std::exception& e) {
        fail(name);
        diag(uni::cat(
            "Threw ", Show<std::exception>().show(e)
        ));
        return false;
    }
    catch (...) {
        fail(name);
        diag("Threw non-standard exception");
        return false;
    }
}

template <class A, class B>
void diag_unexpected(const A& got, const B& expected) {
    diag(uni::cat("Expected ", Show<B>().show(expected)));
    diag(uni::cat("     got ", Show<A>().show(got)));
}

template <class E>
void diag_exception_failed_check(const E& got) {
    diag("Exception failed the check");
    diag(uni::cat("     Got ", Show<E>().show(got)));
}

 // TODO: move to StringConversion
inline
uni::UniqueString type_name (const std::type_info& type) {
#if __has_include(<cxxabi.h>)
    int status;
    char* demangled = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
    if (status != 0) return uni::cat("(Failed to demangle ", type.name(), ")");
    uni::UniqueString r = const_cast<const char*>(demangled);
    free(demangled);
    return r;
#else
     // Probably MSVC, which automatically demangles.
    return type.name();
#endif
}

inline
void diag_didnt_throw (const std::type_info& expected) {
    diag(uni::cat("Expected exception of type ", type_name(expected)));
}
inline
void diag_wrong_exception (const std::exception& got, const std::type_info& expected) {
    diag(uni::cat("Expected exception of type ", type_name(expected)));
    diag(uni::cat("     Got ", Show<std::exception>().show(got)));
}
inline
void diag_wrong_exception_nonstandard (const std::type_info& expected) {
    diag(uni::cat("Expected exception of type ", type_name(expected)));
    diag("     Got non-standard exception.");
}

struct plusminus {
    double range;
    double center;
};

} // in

#ifndef TAP_DISABLE_TESTS
 // TODO: detect duplicate test names
inline
TestSet::TestSet (uni::AnyString n, void(* c )()) :
    name(n), code(c)
{
    in::testers().emplace_back(this);
}
#endif

inline
void plan (unsigned num_tests) {
    in::num_planned = num_tests;
    in::num_tested = 0;
    in::num_to_todo = 0;
    in::print(uni::cat("1..", num_tests, "\n"));
}

inline
void done_testing () {
    plan(in::num_tested);
}

inline
bool ok_bool (bool succeeded, uni::Str name) {
    in::num_tested += 1;
    uni::UniqueString suffix;
    if (in::num_to_todo || in::block_todo) {
        suffix = uni::cat(" # TODO ", in::todo_excuse);
        if (in::num_to_todo) in::num_to_todo--;
    }
    uni::UniqueString m = uni::cat(
        succeeded ? "ok " : "not ok ",
        in::num_tested, ' ', name, suffix, '\n'
    );
    in::print(m);
    return succeeded;
}

template <class F>
bool try_ok (F code, uni::Str name) {
    return in::fail_on_throw([&]{
        return ok(code(), name);
    }, name);
}

template <class A, class B>
bool is (const A& got, const B& expected, uni::Str name) {
    return in::fail_on_throw([&]{
        bool res;
         // Use strcmp if both args coerce to const char* AND there is no custom
         // operator== handling them (the builtin == for pointers is not a
         // function, so it can't be called with operator==().).
        if constexpr (
            requires (const char* p) { p = got; p = expected; } &&
            !requires { operator==(got, expected); }
        ) {
            const char* g = got;
            const char* e = expected;
            if (!g || !e) res = g == e;
            else res = std::strcmp(g, e) == 0;
        }
        else res = got == expected;
        if (res) {
            return pass(name);
        }
        else {
            fail(name);
            in::diag_unexpected(got, expected);
            return false;
        }
    }, name);
}

template <class F, class B>
bool try_is (F code, const B& expected, uni::Str name) {
    return in::fail_on_throw([&]{
        const auto& got = std::forward<F>(code)();
        return is(got, expected, name);
    }, name);
}

template <class A, class B>
bool isnt (const A& got, const B& unexpected, uni::Str name) {
    return in::fail_on_throw([&]{
        bool res;
        if constexpr (
            requires (const char* p) { p = got; p = unexpected; } &&
            !requires { operator==(got, unexpected); }
        ) {
            const char* g = got;
            const char* e = unexpected;
            if (!g || !e) res = g != e;
            else res = std::strcmp(g, e) != 0;
        }
        else res = got != unexpected;
        return ok(res, name);
    }, name);
}

template <class F, class B>
bool try_isnt (F code, const B& unexpected, uni::Str name) {
    return in::fail_on_throw([&]{
        const auto& got = std::forward<F>(code)();
        return isnt(got, unexpected, name);
    }, name);
}

inline
bool within (double got, double range, double expected, uni::Str name) {
    if (range < 0) range = -range;
    if (got >= expected - range && got <= expected + range) {
        return pass(name);
    }
    else {
        fail(name);
        in::diag_unexpected(got, in::plusminus{range, expected});
        return false;
    }
}

template <class F>
bool try_within (F code, double range, double expected, uni::Str name) {
    return in::fail_on_throw([&]{
        return within(std::forward<F>(code)(), range, expected, name);
    }, name);
}

template <class E, class F>
bool throws (F code, uni::Str name) {
    try {
        std::forward<F>(code)();
        fail(name);
        in::diag_didnt_throw(typeid(E));
        return false;
    }
    catch (const E& e) {
        return pass(name);
    }
    catch (const scary_exception& e) { throw; }
    catch (const std::exception& e) {
        fail(name);
        in::diag_wrong_exception(e, typeid(E));
        return false;
    }
    catch (...) {
        fail(name);
        in::diag_wrong_exception_nonstandard(typeid(E));
        return false;
    }
}

template <class E, class F>
bool throws_is (F code, const E& expected, uni::Str name) {
    try {
        std::forward<F>(code)();
        fail(name);
        in::diag_didnt_throw(typeid(E));
        return false;
    }
    catch (const E& e) {
        if (e == expected) {
            return pass(name);
        }
        else {
            fail(name);
            in::diag_unexpected(e, expected);
            return false;
        }
    }
    catch (const scary_exception& e) { throw; }
    catch (const std::exception& e) {
        fail(name);
        in::diag_wrong_exception(e, typeid(E));
        return false;
    }
    catch (...) {
        fail(name);
        in::diag_wrong_exception_nonstandard(typeid(E));
        return false;
    }
}

template <class E, class F>
bool throws_what (F code, uni::Str what, uni::Str name) {
    try {
        std::forward<F>(code)();
        fail(name);
        in::diag_didnt_throw(typeid(E));
        return false;
    }
    catch (const E& e) {
        if (e.what() == what) {
            return pass(name);
        }
        else {
            fail(name);
            in::diag_unexpected(e.what(), what);
            return false;
        }
    }
    catch (const scary_exception& e) { throw; }
    catch (const std::exception& e) {
        fail(name);
        in::diag_wrong_exception(e, typeid(E));
        return false;
    }
    catch (...) {
        fail(name);
        in::diag_wrong_exception_nonstandard(typeid(E));
        return false;
    }
}

template <class E, class F, class P>
bool throws_check (F code, P check, uni::Str name) {
    try {
        std::forward<F>(code)();
        fail(name);
        in::diag_didnt_throw(typeid(E));
        return false;
    }
    catch (const E& e) {
        if (std::forward<P>(check)(e)) {
            return pass(name);
        }
        else {
            fail(name);
            in::diag_exception_failed_check(e);
            return false;
        }
    }
    catch (const scary_exception& e) { throw; }
    catch (const std::exception& e) {
        fail(name);
        in::diag_wrong_exception(e, typeid(E));
        return false;
    }
    catch (...) {
        fail(name);
        in::diag_wrong_exception_nonstandard(typeid(E));
        return false;
    }
}

template <class F>
bool doesnt_throw (F code, uni::Str name) {
    return in::fail_on_throw([&]{
        std::forward<F>(code)();
        return pass(name);
    }, name);
}

inline
bool pass (uni::Str name) {
    return ok(true, name);
}
inline
bool fail (uni::Str name) {
    return ok(false, name);
}
inline
void todo (unsigned num, uni::AnyString excuse) {
    in::num_to_todo = num;
    in::todo_excuse = std::move(excuse);
}
template <class F>
void todo (uni::AnyString excuse, F code) {
    auto old_excuse = std::move(in::todo_excuse);
    auto old_block_todo = in::block_todo;
    in::todo_excuse = std::move(excuse);
    in::block_todo = true;
    std::forward<F>(code)();
    in::todo_excuse = std::move(old_excuse);
    in::block_todo = old_block_todo;
}
inline
void skip (unsigned num, uni::Str excuse) {
    for (unsigned int i = 0; i < num; i++) {
        in::num_tested++;
        in::print(uni::cat(
            "ok ", in::num_tested, " # SKIP ", excuse, '\n'
        ));
    }
}
inline
void set_print (void(* f )(uni::Str)) {
    in::print = f;
}
inline
void diag (uni::Str message) {
    in::print(uni::cat(" # ", message,  '\n'));
}
inline
void BAIL_OUT (uni::Str reason) {
    in::print(uni::cat("Bail out!  ", reason));
    exit(1);
}

///// Default show

template <class T>
uni::UniqueString Show<T>::show (const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    }
    else if constexpr (std::is_same_v<T, char>) {
        return uni::cat('\'', v, '\'');
    }
    else if constexpr (std::is_same_v<T, const char*>) {
        return v ? uni::cat('"', v, '"') : "null";
    }
    else if constexpr (requires (uni::Str s) { s = v; }) {
        return uni::cat('"', v, '"');
    }
    else if constexpr (std::is_same_v<T, uni::Null>) {
        return "null";
    }
    else if constexpr (std::is_same_v<T, std::exception>) {
        return uni::cat(
            "exception of type ", in::type_name(typeid(v)),
            ": ", v.what()
        );
    }
    else if constexpr (std::is_arithmetic_v<T> || std::is_pointer_v<T>) {
        return uni::cat(v);
    }
    else if constexpr (std::is_enum_v<T>) {
        return uni::cat("(enum value) ", std::underlying_type_t<T>(v));
    }
    else if constexpr (std::is_same_v<T, in::plusminus>) {
        return uni::cat(
            "within +/- ", v.range, " of ", v.center
        );
    }
    else {
        return uni::cat(
            "(unprintable object of type ", in::type_name(typeid(T)), ")"
        );
    }
}

inline
void allow_testing (int argc, char** argv, uni::Str test_flag) {
    tap::argc = argc;
    tap::argv = argv;
    if (test_flag) {
        if (argc >= 2 && uni::Str(argv[1]) == test_flag) {
            if (argc >= 3) {
                run_test(argv[2]);
                std::exit(0);
            }
            else {
                list_tests();
                std::exit(0);
            }
        }
        return;  // escape here if no testing arguments.
    }
    else if (argc >= 2) {
        run_test(argv[1]);
        std::exit(0);
    }
    else {
        list_tests();
        std::exit(0);
    }
}

#ifndef TAP_DISABLE_TESTS
inline
void run_test (uni::Str name) {
    for (auto& t : in::testers()) {
        if (t->name == name) {
            try {
                t->code();
            }
            catch (std::exception& e) {
                in::print(uni::cat("Uncaught exception: ", e.what(), '\n'));
                throw;
            }
            catch (...) {
                in::print("Uncaught non-standard exception.\n");
                throw;
            }
            return;
        }
    }
    in::print(uni::cat(
        "1..1\nnot ok 1 - No test named ", name, " has been compiled.\n"
    ));
}
#else
inline
void run_test (uni::Str) {
    in::print("1..0 # SKIP this program was compiled with testing disabled\n");
}
#endif

inline
void list_tests () {
#ifndef TAP_DISABLE_TESTS
    for (auto& t : in::testers()) {
        in::print(uni::cat(t->name, "\n"));
    }
#else
    in::print("(testing disabled)");
#endif
}

}  // namespace tap

