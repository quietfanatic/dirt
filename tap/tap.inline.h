// Implementation details for tap.h

namespace tap {

namespace in {

struct Mu;

 // Global state
inline unsigned num_planned = 0;
inline unsigned num_tested = 0;
inline void(* print )(uni::Str) = [](uni::Str s){
    std::fwrite(s.data(), 1, s.size(), stdout);
};
inline unsigned num_to_todo = 0;
inline bool block_todo = false;
inline uni::AnyString todo_excuse;

 // Internal helpers

uni::UniqueString type_name (const std::type_info& type);
void diag_didnt_throw (const std::type_info& expected);

bool catch_fail (uni::Str name);
bool catch_wrong_exception (const std::type_info& e, uni::Str name);

template <class F>
bool fail_on_throw (F code, uni::Str name) {
    try {
        return std::forward<F>(code)();
    }
    catch (...) { return catch_fail(name); }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"

void diag_unexpected_g (Mu& g, Shower<Mu>& gs, Mu& e, Shower<Mu>& es);
template <class A, class B>
void diag_unexpected (const A& got, const B& expected) {
    diag_unexpected_g(
        (Mu&)got, (Shower<Mu>&)Show<A>::show,
        (Mu&)expected, (Shower<Mu>&)Show<B>::show
    );
}

void diag_exception_failed_check_s (Mu& g, Shower<Mu>& gs);
template <class E>
void diag_exception_failed_check (const E& got) {
    diag_exception_failed_check_s(
        (Mu&)got, (Shower<Mu>&)Show<E>::show
    );
}

#pragma GCC diagnostic pop

struct plusminus {
    double range;
    double center;
};

} // in

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
        if (ok(res)) return true;
        else {
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
    catch (...) { return in::catch_wrong_exception(typeid(E), name); }
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
    catch (...) { return in::catch_wrong_exception(typeid(E), name); }
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
    catch (...) { return in::catch_wrong_exception(typeid(E), name); }
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
    catch (...) { return in::catch_wrong_exception(typeid(E), name); }
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
void set_print (void(* f )(uni::Str)) {
    in::print = f;
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
    else if constexpr (std::is_base_of_v<std::exception, T>) {
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

}  // namespace tap
