#include "description.internal.h"

#include <typeindex>
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

#include "../../uni/hash.h"
#include "descriptors.private.h"
#include "type.h"

namespace ayu {
namespace in {

struct Registry {
    UniqueArray<Pair<usize, const Description*>> by_name;
    bool initted = false;
};

static Registry& registry () {
    static Registry r;
    return r;
}

static void init_names () {
    auto& r = registry();
    if (!r.initted) {
        r.initted = true;
        plog("init types begin");
        for (auto& p : r.by_name) {
            p.first = uni::hash(get_description_name(p.second));
        }
        std::sort(r.by_name.begin(), r.by_name.end(), [](auto& a, auto& b){
            if (a.first == b.first) [[unlikely]] {
                auto an = get_description_name(a.second);
                auto bn = get_description_name(b.second);
                if (an.size() == bn.size()) {
                    return std::memcmp(an.data(), bn.data(), an.size()) < 0;
                }
                else return an.size() < bn.size();
            }
            else return a.first < b.first;
        });
        plog("init types end");
    }
}

const Description* register_description (const Description* desc) noexcept {
    require(!registry().initted);
    registry().by_name.emplace_back(0, desc);
    return desc;
}

const Description* get_description_for_name (Str name) noexcept {
    init_names();
    auto& ds = registry().by_name;
    auto h = uni::hash(name);
    auto bottom = ds.begin();
    auto top = ds.end();
    while (bottom != top) {
        auto mid = bottom + (top - bottom) / 2;
        if (mid->first == h) [[unlikely]] {
            Str n = get_description_name(mid->second);
            if (n.size() == name.size()) [[likely]] {
                if (n == name) [[likely]] {
                    return mid->second;
                }
                else (n < name ? bottom : top) = mid;
            }
            else (n.size() < name.size() ? bottom : top) = mid;
        }
        else (mid->first < h ? bottom : top) = mid;
    }
    return null;
}

const Description* need_description_for_name (Str name) {
    auto desc = get_description_for_name(name);
    if (desc) return desc;
    else raise(e_TypeNotFound, cat(
        "Did not find type named ", name
    ));
}

StaticString get_description_name (const Description* desc) noexcept {
    if (!!(desc->flags & DescFlags::NameComputed)) {
        auto cache = desc->computed_name.cache;
        if (!*cache) {
            AnyString s = desc->computed_name.f();
            *cache = StaticString(s);
            s.impl = {};
        }
        return *cache;
    }
    else if (desc->name) { return desc->name; }
    else {
        return "!(Unknown Type Name)";
    }
}

UniqueString get_demangled_name (const std::type_info& t) noexcept {
#if __has_include(<cxxabi.h>)
    int status;
    char* demangled = abi::__cxa_demangle(t.name(), nullptr, nullptr, &status);
    if (status != 0) return cat("!(Failed to demangle ", t.name(), ')');
    auto r = UniqueString(demangled);
    free(demangled);
    return r;
#else
     // Probably MSVC, which automatically demangles.
    return UniqueString(t.name());
#endif
}

} using namespace in;
} using namespace ayu;
