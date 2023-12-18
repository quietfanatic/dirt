 // A TAP outputting test library for C++
 //
 // Instructions:
 //
 // 1. Declare TestSet object at global scope, either at the bottom of your
 //    ordinary .cpp files or in their own .cpp files.  They should look
 //    something like this.
 //
 //    #include "path/to/tap.h"
 //    static tap::TestSet universe_tests ("universe/universe.cpp" [](){
 //        using namespace tap;
 //        plan(3);
 //        ok(init_universe(), "Everything starts up right");
 //        is(get_answer(), 72, "Just in case");
 //        within(entropy(), 0.1, 0, "Not too hot");
 //    });
 //
 //    Make sure you provide a unique name for each tester.  An easy naming
 //    convention is to use the name of the source file the test is in.
 //
 // 2. There are three ways to run the tests.
 //    A) At the front of your main function, put
 //
 //       tap::allow_testing(argc, argv);
 //
 //       This will cause your program to run tests in response to command-line
 //       arguments.  If you give your program "--test universe/universe.cpp" as
 //       arguments, it will run the TestSet you declared with that name and
 //       then exit.  If you give it "--test" without arguments, it will print a
 //       list of all registered tests, one per line.  Giving a third argument
 //       will make it look for something else instead of "--test".  If you pass
 //       an empty string as the third argument, then running your program with
 //       a single argument or no arguments will produce the above behavior.
 //    B) Have your program call run_test() itself.
 //    C) Compile and link tap.cpp after defining TAP_ENABLE_MAIN.  This will
 //       make tap declare its own main() with just calls tap::allow_testing().
 //       Make sure not to include your own main() or you'll get conflicting
 //       symbols.
 //
 // 3. To run all the tests through a harness, do
 //
 //     ./my_program --test | prove -e "./my_program --test" -
 //
 //    The program 'prove' is generally packaged with Perl.
 //
 // 4. To compile with tests disabled for release, define TAP_DISABLE_TESTS.  If
 //    this is defined, no tests will be registered, and ideally your linker
 //    will discard all testing code.

///// BOILERPLATE

#pragma once

#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <vector>

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

namespace tap {
using namespace std;

///// TestSet

#ifndef TAP_DISABLE_TESTS

 // This is the struct you use to declare a set of tests.
struct TestSet {
     // The constructor will register the test at init-time.
    TestSet (std::string_view name, void(* code )());
};

#else

 // Stub struct for when TAP_DISABLE_TESTS is defined.
struct TestSet {
    TestSet (std::string_view, void(*)()) { }
};

#endif

///// Testing functions

 // Do this at the beginning of your testing.  Give it as an argument the
 //  number of tests you plan to run.  If you run a different number, the
 //  test set is considered to have failed.
void plan (unsigned num_tests);

 // Alternatively, do this at the end of your testing.
void done_testing ();

 // Run a test.  If succeeded is true, the test is successful, otherwise it
 //  is a failure.
bool ok (bool succeeded, std::string_view name = "");
 // The try_* versions of testing functions fail if the code throws an exception.
 //  Otherwise, they behave like the non-try versions with the returned result.
template <class F>
bool try_ok (F code, std::string_view name = "");

 // Run a test that succeeds if got == expected (with overloaded operator ==).
 //  If the test failed, it will try to tell you what it got vs. what it expected.
 // Will fail if the == operator throws an exception.
 // You probably know that you shouldn't use == to compare floating point numbers,
 //  so for those, look at within() and about().
 // As a special case, you can use is() with const char* and it'll do a strcmp (with
 //  NULL checks).
template <class A, class B>
bool is (const A& got, const B& expected, std::string_view name = "");
template <class F, class B>
bool try_is (F code, const B& expected, std::string_view name = "");

 // Unlike is, isnt isn't that useful, but at least it catches exceptions in the != operator.
template <class A, class B>
bool isnt (const A& got, const B& unexpected, std::string_view name = "");
template <class F, class B>
bool try_isnt (F code, const B& unexpected, std::string_view name = "");

 // Tests that got is within +/- range of expected.
bool within (double got, double range, double expected, std::string_view name = "");
template <class F>
bool try_within (F code, double range, double expected, std::string_view name = "");
 // Tests that got is within a factor of .001 of expected.
static inline
bool about (double got, double expected, std::string_view name = "") {
    return within(got, expected*0.001, expected, name);
}
template <class F>
bool try_about (F&& code, double expected, std::string_view name = "") {
    return try_within(std::forward<F>(code), expected*0.001, expected, name);
}

 // Tests that code throws an exception of class Except.  If a different kind of
 //  exception is thrown, the test fails.
template <class E = std::exception, class F>
bool throws (F code, std::string_view name = "");

 // Like above, but fails if the thrown exception does not == expected.
template <class E = std::exception, class F>
bool throws_is (F code, const E& expected, std::string_view name = "");

 // Like above, but checks the exception's what() against a string.
template <class E = std::exception, class F>
bool throws_what (F code, std::string_view what, std::string_view name = "");

 // Like above, but fails if the thrown exception does not satisfy the predicate
 // 'check' which should take an E and return bool.
template <class E = std::exception, class F, class P>
bool throws_check (F code, P check, std::string_view name = "");

 // Succeeds if no exception is thrown.
template <class F>
bool doesnt_throw (F code, std::string_view name = "");

 // Automatically pass a test with this name.  Only resort to this if you can't
 //  make your test work with the other testing functions.
bool pass (std::string_view name = "");
 // Likewise with fail.
bool fail (std::string_view name = "");

 // Alias for doesnt_throw
template <class F>
bool try_pass (F code, std::string_view name = "") {
    return doesnt_throw(code, name);
}

 // Mark the next num tests as todo.  You must still run the tests.  If only
 //  todo tests fail, the test set is still considered successful.
void todo (unsigned num, std::string_view excuse = "");
 // Just todo one test.
static inline void todo (std::string_view excuse = "") {
    todo(1, excuse);
}
 // The block form marks as todo every test that runs inside it.  It can be safely
template <class F>
void todo (std::string_view excuse, F code);

 // Declare that you've skipped num tests.  You must NOT still run the tests.
void skip (unsigned num, std::string_view excuse = "");
 // Just skip one test.
static inline void skip (std::string_view excuse = "") {
    skip(1, excuse);
}

///// DIAGNOSTICS

 // Tap will use this to print strings to stdout.  By default, this is
 //     fwrite(s.data(), 1, s.size(), stdout);
void set_print (void(*)(std::string_view));

 // Convert an arbitrary item to a string.  Feel free to overload this for your
 //  own types.  Throwing exceptions from show() may cause duplicate test failures.
 // TODO: allow wholesale replacement of showing for ayu
template <class T>
struct Show {
    std::string show (const T&);
};

 // Print a message as diagnostics.  Should not contain newlines.
void diag (std::string_view message);

///// UH-OH

 // When everything is wrong and you can't even continue testing.  Immediately fails
 //  the whole test set and calls exit(1).
void BAIL_OUT (std::string_view reason = "");

 // Testing functions normally catch exceptions, but they won't catch ones that
 //  inherit from this (unless it's a throws<>() and the exception matches it).
struct scary_exception : std::exception { };

///// RUNNING TESTS

 // Do this in main to allow command-line testing.
void allow_testing (int argc, char** argv, std::string_view test_flag = "--test");

 // To run a test set manually, do this.  It will not exit unless BAIL_OUT is called.
void run_test (std::string_view name);
 // To list the tests manually, do this.  It will print test set names to stdout.
void list_tests ();

 // Copies of the parameters passed to allow_testing that you can access from
 //  your tests.  These are not available if you directly call run_test.
inline int argc;
inline char** argv;

}  // namespace tap

#include "tap.inline.h"
