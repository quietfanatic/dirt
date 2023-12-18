// Implementation details for tap.h

namespace tap {

namespace in {

struct TestSetData {
    std::string name;
    void(* code )();
};

inline
std::vector<std::unique_ptr<TestSetData>>& testers () {
    static std::vector<std::unique_ptr<TestSetData>> testers;
    return testers;
}

 // Global state
inline unsigned num_planned = 0;
inline unsigned num_tested = 0;
inline unsigned num_to_todo = 0;
inline bool block_todo = false;
inline std::string todo_excuse;
inline void(* print )(std::string_view) = [](std::string_view s){
    fwrite(s.data(), 1, s.size(), stdout);
};

 // Internal helpers

template <class F>
bool fail_on_throw (F code, std::string_view name) {
    try {
        return std::forward<F>(code)();
    }
    catch (const scary_exception&) { throw; }
    catch (const std::exception& e) {
        fail(name);
        diag("Threw " + Show<std::exception>().show(e));
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
    diag("Expected " + Show<B>().show(expected));
    diag("     got " + Show<A>().show(got));
}

template <class E>
void diag_exception_failed_check(const E& got) {
    diag("Exception failed the check");
    diag("     Got " + Show<E>().show(got));
}

inline
std::string type_name (const type_info& type) {
#if __has_include(<cxxabi.h>)
    int status;
    char* demangled = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
    if (status != 0) return "(Failed to demangle "s + type.name() + ")";
    std::string r = const_cast<const char*>(demangled);
    free(demangled);
    return r;
#else
     // Probably MSVC, which automatically demangles.
    return std::string(type.name());
#endif
}

inline
void diag_didnt_throw (const std::type_info& expected) {
    diag("Expected exception of type " + type_name(expected));
}
inline
void diag_wrong_exception (const std::exception& got, const std::type_info& expected) {
    diag("Expected exception of type " + type_name(expected));
    diag("     Got " + Show<std::exception>().show(got));
}
inline
void diag_wrong_exception_nonstandard (const std::type_info& expected) {
    diag("Expected exception of type " + type_name(expected));
    diag("     Got non-standard exception.");
}
inline
std::string show_ptr (void* v) {
    if (v) {
        std::string r (2 + sizeof(void*) * 2, 0);
        r[0] = '0'; r[1] = 'x';
        for (unsigned i = 0; i < sizeof(void*) * 2; i++) {
            int nyb = (std::size_t(v) >> (sizeof(void*) * 8 - (i+1) * 4)) & 0xf;
            r[i+2] = nyb < 10 ? '0' + nyb : 'a' + nyb - 10;
        }
        return r;
    }
    else {
        return "nullptr";
    }
}

struct plusminus {
    double range;
    double center;
};

} // in

 // TODO: detect duplicate test names
inline
TestSet::TestSet (std::string_view name, void(* code )()) {
    in::testers().emplace_back(new in::TestSetData{std::string(name), code});
}

inline
void plan (unsigned num_tests) {
    in::num_planned = num_tests;
    in::num_tested = 0;
    in::num_to_todo = 0;
    in::print("1.." + std::to_string(num_tests) + "\n");
}

inline
void done_testing () {
    plan(in::num_tested);
}

inline
bool ok (bool succeeded, std::string_view name) {
    in::num_tested += 1;
    std::string m;
    if (!succeeded) {
        m += "not ";
    }
    m += "ok "; m += std::to_string(in::num_tested);
    if (!name.empty()) {
        m += " "; m += name;
    };
    if (in::num_to_todo || in::block_todo) {
        m += " # TODO "; m += in::todo_excuse;
        if (in::num_to_todo) in::num_to_todo--;
    }
    in::print(m += "\n");
    return succeeded;
}

template <class F>
bool try_ok (F code, std::string_view name) {
    return in::fail_on_throw([&]{
        return ok(code(), name);
    }, name);
}

template <class A, class B>
bool is (const A& got, const B& expected, std::string_view name) {
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
bool try_is (F code, const B& expected, std::string_view name) {
    return in::fail_on_throw([&]{
        const auto& got = std::forward<F>(code)();
        return is(got, expected, name);
    }, name);
}

template <class A, class B>
bool isnt (const A& got, const B& unexpected, std::string_view name) {
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
bool try_isnt (F code, const B& unexpected, std::string_view name) {
    return in::fail_on_throw([&]{
        const auto& got = std::forward<F>(code)();
        return isnt(got, unexpected, name);
    }, name);
}

inline
bool within (double got, double range, double expected, std::string_view name) {
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
bool try_within (F code, double range, double expected, std::string_view name) {
    return in::fail_on_throw([&]{
        return within(std::forward<F>(code)(), range, expected, name);
    }, name);
}

template <class E, class F>
bool throws (F code, std::string_view name) {
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
bool throws_is (F code, const E& expected, std::string_view name) {
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
bool throws_what (F code, std::string_view what, std::string_view name) {
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
bool throws_check (F code, P check, std::string_view name) {
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
bool doesnt_throw (F code, std::string_view name) {
    return in::fail_on_throw([&]{
        std::forward<F>(code)();
        return pass(name);
    }, name);
}

inline
bool pass (std::string_view name) {
    return ok(true, name);
}
inline
bool fail (std::string_view name) {
    return ok(false, name);
}
inline
void todo (unsigned num, std::string_view excuse) {
    in::num_to_todo = num;
    in::todo_excuse = excuse;
}
template <class F>
void todo (std::string_view excuse, F code) {
    auto old_excuse = in::todo_excuse;
    auto old_block_todo = in::block_todo;
    in::todo_excuse = excuse;
    in::block_todo = true;
    std::forward<F>(code)();
    in::todo_excuse = old_excuse;
    in::block_todo = old_block_todo;
}
inline
void skip (unsigned num, std::string_view excuse) {
    for (unsigned int i = 0; i < num; i++) {
        in::num_tested++;
        in::print(
            "ok " + std::to_string(in::num_tested) +
            " # SKIP " + std::string(excuse) + "\n"
        );
    }
}
inline
void set_print (void(* f )(std::string_view)) {
    in::print = f;
}
inline
void diag (std::string_view message) {
    in::print(" # " + std::string(message) + "\n");
}
inline
void BAIL_OUT (std::string_view reason) {
    in::print("Bail out!  " + std::string(reason));
    exit(1);
}

///// Default show

template <class T>
std::string Show<T>::show (const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    }
    else if constexpr (std::is_same_v<T, char>) {
        return std::string("'") + v + "'";
    }
     // TODO: Generalize to anything that stringifies
    else if constexpr (std::is_same_v<T, const char*>) {
        return v ? "\"" + std::string(v) + "\"" : "nullptr";
    }
    else if constexpr (
        std::is_array_v<T> &&
        std::is_same_v<std::remove_cvref_t<std::remove_extent_t<T>>, char>
    ) {
        return "\"" + std::string(v) + "\"";
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        return "\"" + v + "\"";
    }
    else if constexpr (std::is_same_v<T, std::string_view>) {
        return "\"" + std::string(v) + "\"";
    }
    else if constexpr (std::is_same_v<T, std::nullptr_t>) {
        return "nullptr";
    }
    else if constexpr (std::is_same_v<T, std::exception>) {
        return "exception of type " + in::type_name(typeid(v)) + ": " + v.what();
    }
    else if constexpr (std::is_arithmetic_v<T>) {
        return std::to_string(v);
    }
    else if constexpr (std::is_pointer_v<T>) {
        return in::show_ptr((void*)v);
    }
    else if constexpr (std::is_enum_v<T>) {
        return "(enum value) " + std::to_string(std::underlying_type_t<T>(v));
    }
    else if constexpr (std::is_same_v<T, in::plusminus>) {
        return "within +/- " + Show<double>().show(v.range) + " of " + Show<double>().show(v.center);
    }
    else {
        return "(unprintable object of type " + in::type_name(typeid(T)) + ")";
    }
}

inline
void allow_testing (int argc, char** argv, std::string_view test_flag) {
    tap::argc = argc;
    tap::argv = argv;
    if (test_flag.size()) {
        if (argc >= 2 && argv[1] == test_flag) {
            if (argc >= 3) {
                run_test(argv[2]);
                exit(0);
            }
            else {
                list_tests();
                exit(0);
            }
        }
        return;  // escape here if no testing arguments.
    }
    else if (argc >= 2) {
        run_test(argv[1]);
        exit(0);
    }
    else {
        list_tests();
        exit(0);
    }
}

inline
void run_test (std::string_view name) {
#ifndef TAP_DISABLE_TESTS
    for (auto& t : in::testers()) {
        if (t->name == name) {
            try {
                t->code();
            }
            catch (std::exception& e) {
                in::print("Uncaught exception: " + std::string(e.what()) + "\n");
                throw;
            }
            catch (...) {
                in::print("Uncaught non-standard exception.\n");
                throw;
            }
            return;
        }
    }
    in::print("1..1\nnot ok 1 - No test named " + std::string(name) + " has been compiled.\n");
#else
    in::print("1..0 # SKIP this program was compiled with testing disabled\n");
#endif
}

inline
void list_tests () {
#ifndef TAP_DISABLE_TESTS
    for (auto& t : in::testers()) {
        in::print(t->name + "\n");
    }
#else
    in::print("(testing disabled)");
#endif
}

}  // namespace tap

