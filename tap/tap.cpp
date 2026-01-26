#include "tap.h"

#ifdef TAP_SELF_TEST
#ifndef TAP_DISABLE_TESTS

#include <string>

namespace tap {

namespace in {

inline
uni::UniqueArray<const TestSet*>& testers () {
    static uni::UniqueArray<const TestSet*> testers;
    return testers;
}

 // TODO: move to StringConversion
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

void diag_didnt_throw (const std::type_info& expected) {
    diag(uni::cat("Expected exception of type ", type_name(expected)));
}

void diag_unexpected_g (Mu& g, Shower<Mu>& gs, Mu& e, Shower<Mu>& es) {
    diag(uni::cat("Expected ", gs(g)));
    diag(uni::cat("     got ", es(e)));
}

void diag_exception_failed_check_s (Mu& g, Shower<Mu>& gs) {
    diag(uni::cat("     Got ", gs(g)));
}

bool catch_fail (uni::Str name) {
    try {
         // This is silly because it creates a refcounted ptr just to throw it,
         // but it's the only API to rethrow an exception without knowing
         // anything about its type.
        std::rethrow_exception(std::current_exception());
    }
    catch (const scary_exception&) { throw; }
    catch (const std::exception& e) {
        fail(name);
        diag(uni::cat(
            "Threw ", Show<std::exception>::show(e)
        ));
        return false;
    }
    catch (...) {
        fail(name);
        diag("Threw non-standard exception");
        return false;
    }
}

bool catch_wrong_exception (const std::type_info& t, uni::Str name) {
    try { std::rethrow_exception(std::current_exception()); }
    catch (const scary_exception& e) { throw; }
    catch (const std::exception& e) {
        fail(name);
        diag(uni::cat("Expected exception of type ", type_name(t)));
        diag(uni::cat("     Got ", Show<std::exception>::show(e)));
        return false;
    }
    catch (...) {
        fail(name);
        diag(uni::cat("Expected exception of type ", type_name(t)));
        diag("     Got non-standard exception.");
        return false;
    }
}

} // in

#ifndef TAP_DISABLE_TESTS
 // TODO: detect duplicate test names
TestSet::TestSet (uni::AnyString n, void(* c )()) :
    name(n), code(c)
{
    in::testers().emplace_back(this);
}
#endif

void plan (unsigned num_tests) {
    in::num_planned = num_tests;
    in::num_tested = 0;
    in::num_to_todo = 0;
    in::print(uni::cat("1..", num_tests, "\n"));
}

void done_testing () {
    plan(in::num_tested);
}

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

void skip (unsigned num, uni::Str excuse) {
    for (unsigned int i = 0; i < num; i++) {
        in::num_tested++;
        in::print(uni::cat(
            "ok ", in::num_tested, " # SKIP ", excuse, '\n'
        ));
    }
}

void diag (uni::Str message) {
    in::print(uni::cat(" # ", message,  '\n'));
}

void BAIL_OUT (uni::Str reason) {
    in::print(uni::cat("Bail out!  ", reason));
    exit(1);
}

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
void run_test (uni::Str) {
    in::print("1..0 # SKIP this program was compiled with testing disabled\n");
}
#endif

void list_tests () {
#ifndef TAP_DISABLE_TESTS
    for (auto& t : in::testers()) {
        in::print(uni::cat(t->name, "\n"));
    }
#else
    in::print("(testing disabled)");
#endif
}

///// Self tests

static tap::TestSet tap_self_tests ("dirt/tap/tap", []{
    using namespace tap;
    plan(49);

    pass("pass passes");
    ok(true, "ok on true passes");
    try_ok([]{return true;}, "try_ok works");
    is((int)32, (int)32, "is on equal ints passes");
    try_is([]{return 32;}, 32, "try_is works");
    is((float)32, (float)32, "is on equal floats passes");
    is((double)32, (double)32, "is on equal floats passes");
    is("asdf", "asdf", "is strcmp on equal strings passes");
    try_is([]{return "asdf";}, "asdf", "try_is strcmp works");
    is((const char*)NULL, (const char*)NULL, "is strcmp on NULLS passes");
    is((int*)NULL, (int*)NULL, "is on int* NULLS passes");
    int heyguys = 9;
    is(&heyguys, &heyguys, "is can compare pointers");
    is(std::string("asdf"), std::string("asdf"), "is on equal std::strings passes");
    is(std::string("asdf"), "asdf", "is on equal std::string and const char* passes");
    within(1.0, 0.1, 1.001, "within can pass");
    try_within([]{return 1.4;}, 0.1, 1.399, "try_within works");
    about(1.0, 1.001, "about can pass");
    try_about([]{return 1.4;}, 1.4004, "try_about can take functions");
    about(-25, -25.003, "about can take negative numbers");
    doesnt_throw([]{}, "doesnt_throw can pass");
    throws<int>([]{throw (int)3;}, "throws<int> can pass");
    throws_is([]{throw (int)3;}, 3, "throws_is can compare the exception");
    throws_check<int>([]{throw (int)3;}, [](int x){return x==3;}, "throws_check can test the exception");
    struct bad : scary_exception { };
    throws<bad>([]{
        try_ok([]{throw bad{}; return true;}, "Shouldn't reach this");
        fail("Shouldn't reach this");
    }, "bail_out_exception skips normal handlers but is caught by throws<bail_out_exception>()");

    skip("Pretend to skip a test");
    skip(6, "Pretend to skip 6 tests");
    todo("Testing todo (and failures)");
    fail("fail fails");
    todo(2, "Testing numeric todo (and failures)");
    ok(false, "ok on false fails");
    try_ok([]{return false;}, "try_ok can fail");
    todo("Testing block todo (and failures)", [&]{
        is((int)5, (int)3245, "is can fail");
        is("asdf", "fdsa", "is strcmp can fail");
        is("sadf", nullptr, "is strcmp fails on single NULL");
        is((const char*)nullptr, "sadf", "is strcmp fails on single NULL");
        int nope = -9999;
        is(&heyguys, &nope, "is fails on different pointers");
        is(std::string("sadf"), std::string("qwert"), "is fails on different std::strings");
        within(1.0, 0.1, 1.11, "within can fail");
        try_within([]{return 1.4;}, 0.3, 1, "try_within can fail");
        about(1.0, 1.1, "about can fail");
        doesnt_throw([]{throw std::logic_error("ACK");}, "doesnt_throw catches and fails on exception");
        throws<int>([]{ }, "throws fails when no exception is thrown");
        throws<int>([]{throw std::logic_error("ACK");}, "throws fails on wrong kind of exception");
        throws_check<int>([]{throw (int)3;}, [](int x){return x==5;}, "throws can fail the exception test");
        try_ok([]{
            throw std::logic_error("false");
            return true;
        }, "try_ok catches and fails on exception");
        try_is([]{
            throw std::logic_error("X");
            return 32;
        }, 32, "try_is catches and fails on exception");
    });
});

} // tap

#endif
#endif

#ifdef TAP_ENABLE_MAIN
int main (int argc, char** argv) {
    tap::allow_testing(argc, argv);
    return 0;
}
#endif
