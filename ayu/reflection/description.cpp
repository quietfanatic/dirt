#include "description.internal.h"

#include <typeindex>
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

#include "descriptors.private.h"
#include "type.h"

namespace ayu {
namespace in {

struct Registry {
    UniqueArray<const Description*> to_init;
    std::unordered_map<Str, const Description*> by_name;
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
        r.to_init.consume([&r](const Description* desc){
            r.by_name.emplace(get_description_name(desc), desc);
        });
        plog("init types end");
    }
}

const Description* register_description (const Description* desc) noexcept {
    require(!registry().initted);
    registry().to_init.push_back(desc);
    return desc;
}

const Description* get_description_for_name (Str name) noexcept {
    init_names();
    auto& ds = registry().by_name;
    auto iter = ds.find(name);
    if (iter != ds.end()) return iter->second;
    else return null;
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
