#include "arrays.h"
#include "strings.h"

#include <unordered_map>
#include <vector>

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

using namespace uni;

AnyArray<int> t (AnyArray<int>&& a) {
    return a;
}
AnyArray<int> t2 (const AnyArray<int>& a) {
    return a;
}

UniqueArray<int> t3 (int* dat, usize len) {
    return UniqueArray<int>(dat, len);
}

std::vector<int> c3 (int* dat, usize len) {
    return std::vector<int>(dat, dat+len);
}

UniqueArray<int> t4 (int* a, int* b) {
    return UniqueArray<int>(a, b);
}

UniqueArray<char> t5 (char* a, char* b) {
    return UniqueArray<char>(a, b);
}

AnyArray<int> t6 (const UniqueArray<int>& a) {
    return a;
}
AnyArray<int> t7 (const UniqueArray<int>& a) {
    return AnyArray<int>(a);
}

AnyArray<int> t8 (const std::vector<int>& v) {
    return UniqueArray<int>(v);
}

void t9 (AnyArray<int>& a, const AnyArray<int>& b) {
    a = b;
}
void c9 (std::vector<int>& a, const std::vector<int>& b) {
    a = b;
}

constexpr int foos [] = {2, 4, 6, 8, 10, 12};

AnyArray<int> t10 () {
    return AnyArray<int>(Slice<int>(foos));
}

AnyString t11 () {
    return "formidable";
}

AnyArray<std::pair<usize, usize>> t12 (const std::unordered_map<int, int>& m) {
    auto r = AnyArray<std::pair<usize, usize>>(m.begin(), m.end());
    return r;
}
AnyArray<std::pair<usize, usize>> t13 (const std::unordered_map<int, int>& m) {
    auto r = AnyArray<std::pair<usize, usize>>(m.begin(), m.size());
    return r;
}

void t14 (AnyArray<int>& v) {
    v.reserve(50);
}

void t15 (AnyArray<int>& v) {
    v.shrink_to_fit();
}

void t16 (AnyArray<int>& v) {
    v.make_unique();
}

void t17 (UniqueArray<int>& v) {
    v.resize(50);
}
void c17 (std::vector<int>& v) {
    v.resize(50);
}
void t18 (AnyArray<int>& v) {
    v.resize(50);
}

void t19 (AnyArray<int>& v) {
    v.push_back(99);
}
void c19 (std::vector<int>& v) {
    v.push_back(99);
}
void t20 (UniqueArray<int>& v) {
    v.push_back(99);
}
void t21 (AnyArray<int>& v) {
    v.pop_back();
}

AnyArray<int> t22 () {
    AnyArray<int> r;
    r.reserve(32);
    for (usize i = 0; i < 32; i++) {
        r.emplace_back(i);
    }
    return r;
}

AnyArray<int> b22 () {
    AnyArray<int> r;
    r.reserve(32);
    for (usize i = 0; i < 32; i++) {
        r.emplace_back_expect_capacity(i);
    }
    return r;
}

AnyArray<int> bb22 () {
    AnyArray<int> r (Capacity(32));
    for (usize i = 0; i < 32; i++) {
        r.emplace_back_expect_capacity(i);
    }
    return r;
}

std::vector<int> c22 () {
    std::vector<int> r;
    r.reserve(32);
    for (usize i = 0; i < 32; i++) {
        r.emplace_back(i);
    }
    return r;
}

UniqueArray<int> t23 () {
    UniqueArray<int> r;
    r.reserve(32);
    for (usize i = 0; i < 32; i++) {
        r.emplace_back(i);
    }
    return r;
}

UniqueArray<int> b23 () {
    UniqueArray<int> r;
    r.reserve(32);
    for (usize i = 0; i < 32; i++) {
        r.emplace_back_expect_capacity(i);
    }
    return r;
}

UniqueArray<int> t23b () {
    UniqueArray<int> r (Capacity(32));
    for (usize i = 0; i < 32; i++) {
        r.emplace_back_expect_capacity(i);
    }
    return r;
}

UniqueArray<int> t23c () {
    return UniqueArray<int>(32, [](usize i){ return i; });
}

UniqueArray<int> t23d (usize s) {
    return UniqueArray<int>(s, [](usize i){ return i; });
}

void t24 (UniqueArray<int>& a) {
    a.emplace(32, 100);
}
void c24 (std::vector<int>& a) {
    a.emplace(a.begin() + 32, 100);
}

void t25 (AnyArray<int>& a) {
    a.erase(44, 2);
}

void b25 (UniqueArray<int>& a) {
    a.erase(44, 2);
}

const char* t26 (AnyArray<char>& a) {
    return a.c_str();
}

std::string c29 (std::string&& s) {
    return move(s) + "foo" + "bar";
}

UniqueString t29 (UniqueString&& s) {
    return cat(move(s), "foo", "bar");
}
UniqueString b29 (const UniqueString& s) {
    return cat(s, "foo", "bar");
}

UniqueString t28 (UniqueString&& s) {
    return cat(move(s), "foo", "bar");
}

UniqueString t27 (const char* a, const char* b) {
    return cat(a, b);
}

UniqueString t30 () {
    return cat("foo", 4, "bar");
}

UniqueString b30 (uint32 v) {
    return cat("foo", v, "barbarbar");
}

UniqueString c30 (uint64 v) {
    return cat(v);
}

UniqueString t31 () {
    return cat("foo", 5.0, "bar");
}

UniqueString t32 (double d) {
    return cat("foo", d, "bar");
}

NOINLINE
void t33a (AnyString&& a) {
    printf("%s\n", a.c_str());
}
NOINLINE
void t33b (AnyString a) {
    t33a(move(a));
}
void t33c (AnyString&& a) {
    t33b(move(a));
}
NOINLINE
void c33a (std::string&& a) {
    printf("%s\n", a.c_str());
}
NOINLINE
void c33b (std::string a) {
    c33a(move(a));
}
void c33c (std::string&& a) {
    c33b(move(a));
}

//UniqueString t34a (StaticString a, AnyString b, StaticString c) {
//    return cat("Couldn't ", a, " ", b, " when its state is ", c);
//}
[[gnu::cold]] NOINLINE
UniqueString t34b (StaticString a, AnyString b, StaticString c) {
    return cat("Couldn't ", a, " ", b, " when its state is ", c);
}
[[gnu::cold]]
UniqueString t34c (StaticString a, AnyString b, StaticString c) {
    auto r = UniqueString(Capacity(a.size() + b.size() + c.size() + 29));
    r.append_expect_capacity("Couldn't ");
    r.append_expect_capacity(a);
    r.append_expect_capacity(" ");
    r.append_expect_capacity(b);
    r.append_expect_capacity(" when its state is ");
    r.append_expect_capacity(c);
    return r;
}
[[gnu::cold]]
std::string c34 (std::string_view a, std::string b, std::string_view c) {
    std::string r = "Couldn't ";
    r.append(a);
    r.append(" ");
    r.append(b);
    r.append(" when its state is ");
    r.append(c);
    return r;
}
[[gnu::cold]]
auto b34 (StaticString a, AnyString b, StaticString c) {
    return UniqueArray<std::pair<AnyString, AnyString>>{
        {"tried", a},
        {"name", move(b)},
        {"state", c}
    };
}
[[gnu::cold]]
auto b34b (StaticString a, AnyString b, StaticString c) {
    auto r = UniqueArray<std::pair<AnyString, AnyString>>(Capacity(3));
    r.emplace_back_expect_capacity("tried", a);
    r.emplace_back_expect_capacity("name", move(b));
    r.emplace_back_expect_capacity("state", c);
    return r;
}

UniqueArray<AnyString> t35 () {
    return UniqueArray<AnyString>(555);
}

UniqueArray<AnyString> t36 () {
    return UniqueArray<AnyString>(555, "indestructible");
}

[[gnu::used]]
bool t37 (Str a, Str b) {
    return a == b;
}

static tap::TestSet tests ("dirt/uni/arrays", []{
    using namespace tap;
    AnyArray<int> a;
    is(a.size(), usize(0), "empty array has size 0");
    is(a.data(), null, "empty-constructed array has null data");
    AnyArray<int> b = move(a);
    is(b.size(), usize(0), "move empty array");
    is(b.data(), null);
    AnyArray<int> c = b;
    is(c.size(), usize(0), "copy empty array");
    is(c.data(), null);

    c.push_back(4);
    is(c.size(), usize(1), "push_back");
    is(c[0], 4);
    for (usize i = 0; i < 50; i++) {
        c.push_back(i);
    }
    is(c.size(), usize(51));
    is(c[50], 49);

    is(c.unique(), true, "unique");
    AnyArray<AnyArray<int>> d (5, c);
    is(d.size(), usize(5), "array with non-trivial type");
    is(c.unique(), false, "AnyArray buffer is not copied when AnyArray is copied");
    c.erase(1, 5);
    is(c.unique(), true, "copy on write");
    is(c.size(), usize(46), "erase");
    is(c[1], 5);
    is(d[0][1], 0, "other arrays sharing buffer are not changed");
    is(cat("foo"_s, 6, "bar"_s), "foo6bar"_s, "cat()");
    is(cat("foo", 6, "bar"), "foo6bar", "cat() (raw)");
    is(t34b("save", "bap:/bup#bep", "UNLOADED"), "Couldn't save bap:/bup#bep when its state is UNLOADED");

    done_testing();
});

#endif
