#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"
#include "floating.h"

using namespace geo;


#define TEST_TYPE(T) { \
     /* defined */ \
    ok(!defined(T(GNAN)), #T " defined(GNAN)"); \
    ok(defined(T(-GINF)), #T " defined(-GINF)"); \
    ok(defined(T(0)), #T " defined(0)"); \
    ok(defined(T(GINF)), #T " defined(GINF)"); \
     /* finite */ \
    ok(!finite(T(GNAN)), #T " finite(GNAN)"); \
    ok(!finite(T(-GINF)), #T " finite(-GINF)"); \
    ok(finite(std::numeric_limits<T>::lowest()), #T " finite(lowest)"); \
    ok(finite(std::numeric_limits<T>::max()), #T " finite(max)"); \
    ok(!finite(T(GINF)), #T " finite(GINF)"); \
     /* exact_eq */ \
    ok(exact_eq(T(GNAN), T(GNAN)), #T " exact_eq(GNAN, GNAN)"); \
    ok(!exact_eq(T(GNAN), T(0)), #T " exact_eq(GNAN, 0)"); \
    ok(!exact_eq(T(GNAN), T(GINF)), #T " exact_eq(GNAN, GINF)"); \
    ok(exact_eq(T(-0.0), T(-0.0)), #T " exact_eq(-0, -0)"); \
    ok(!exact_eq(T(-0.0), T(0)), #T " exact_eq(-0, 0)"); \
     /* root2 */ \
    ok(!defined(root2(T(GNAN))), #T " root2(GNAN)"); \
    ok(!defined(root2(T(-GINF))), #T " root2(-GINF)"); \
    ok(!defined(root2(T(-1))), #T " root2(-1)"); \
    ok(exact_eq(root2(T(-0.0)), T(-0.0)), #T " root2(-0)"); \
    ok(exact_eq(root2(T(0)), T(0)), #T " root2(0)"); \
    is(root2(T(1)), T(1), #T " root2(1)"); \
    is(root2(T(4)), T(2), #T " root2(4)"); \
    is(root2(T(GINF)), T(GINF), #T " root2(GINF)"); \
     /* slow_root2 */ \
    ok(!defined(slow_root2(T(GNAN))), #T " slow_root2(GNAN)"); \
    ok(!defined(slow_root2(T(-GINF))), #T " slow_root2(-GINF)"); \
    ok(!defined(slow_root2(T(-1))), #T " slow_root2(-1)"); \
    ok(exact_eq(slow_root2(T(-0.0)), T(-0.0)), #T " slow_root2(-0)"); \
    ok(exact_eq(slow_root2(T(0)), T(0)), #T " slow_root2(0)"); \
    is(slow_root2(T(1)), T(1), #T " slow_root2(1)"); \
    is(slow_root2(T(4)), T(2), #T " slow_root2(4)"); \
    is(slow_root2(T(GINF)), T(GINF), #T " slow_root2(GINF)"); \
}

static tap::TestSet tests ("dirt/geo/floating", []{
    using namespace tap;

    TEST_TYPE(float)
    TEST_TYPE(double)
    done_testing();
});

#endif
