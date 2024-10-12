#include "tap-std.h"

#ifdef TAP_SELF_TEST
#ifndef TAP_DISABLE_TESTS

#include <string>

namespace tap {

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
