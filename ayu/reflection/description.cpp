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
    UniqueArray<Hashed<const Description*>> by_name;
    bool initted = false;
};

static Registry& registry () {
    static Registry r;
    return r;
}

static
StaticString get_description_name_cached (const Description* desc) {
    if (!!(desc->flags & DescFlags::NameComputed)) {
        return *desc->computed_name.cache;
    }
    else return desc->name;
}

NOINLINE static
void init_names () {
    auto& r = registry();
    r.initted = true;
    plog("init types begin");
    for (auto& p : r.by_name) {
        auto n = get_description_name(p.value);
        require(n);
        p.hash = uni::hash(n);
    }
     // Why we using qsort instead of std::sort, isn't std::sort faster?  Yes,
     // but the reason it's faster is that it generates 10k of code for every
     // callsite, which is overkill for something that'll be called exactly
     // once at init time.
    std::qsort(
        r.by_name.data(), r.by_name.size(), sizeof(r.by_name[0]),
        [](const void* aa, const void* bb){
            auto a = reinterpret_cast<const Hashed<const Description*>*>(aa);
            auto b = reinterpret_cast<const Hashed<const Description*>*>(bb);
            if (a->hash != b->hash) [[likely]] {
                 // can't subtract here, it'll overflow
                return a->hash < b->hash ? -1 : 1;
            }
            auto an = get_description_name_cached(a->value);
            auto bn = get_description_name_cached(b->value);
            if (an.size() == bn.size()) {
                return std::memcmp(an.data(), bn.data(), an.size());
            }
            else return int(an.size() - bn.size());
        }
    );
    plog("init types end");
}

const Description* register_description (const Description* desc) noexcept {
    require(!registry().initted);
    registry().by_name.emplace_back(0, desc);
    return desc;
}

 // in current gcc, this optimization interferes with conditional moves
[[gnu::optimize("-fno-thread-jumps")]]
const Description* get_description_for_name (Str name) noexcept {
    auto& r = registry();
    if (!r.initted) [[unlikely]] init_names();
    if (!name) return null;
    auto h = uni::hash(name);
    u32 bottom = 0;
    u32 top = r.by_name.size();
    while (bottom != top) {
        u32 mid = (top + bottom) / 2;
        auto& e = r.by_name[mid];
        if (e.hash == h) [[unlikely]] {
            Str n = get_description_name_cached(e.value);
            if (n == name) [[likely]] {
                return e.value;
            }
            else if (n.size() == name.size()) {
                (n < name ? bottom : top) = mid;
            }
            else (n.size() < name.size() ? bottom : top) = mid;
        }
        else {
            bool up = e.hash < h;
            if (up) bottom = mid + 1;
            if (!up) top = mid;
        }
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
