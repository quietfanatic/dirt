#include "description.internal.h"

#include <typeindex>
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

#include "descriptors.internal.h"
#include "type.h"

namespace ayu {
namespace in {

struct Registry {
    std::unordered_map<std::type_index, const Description*> by_cpp_type;
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
        for (auto& p : r.by_cpp_type) {
            r.by_name.emplace(get_description_name(p.second), p.second);
        }
    }
}

const Description* register_description (const Description* desc) noexcept {
    require(!registry().initted);
    auto [p, e] = registry().by_cpp_type.emplace(*desc->cpp_type, desc);
    return p->second;
}

const Description* get_description_for_type_info (const std::type_info& t) noexcept {
    auto& ds = registry().by_cpp_type;
    auto iter = ds.find(t);
    if (iter != ds.end()) return iter->second;
    else return null;
}
const Description* need_description_for_type_info (const std::type_info& t) {
    auto desc = get_description_for_type_info(t);
    if (desc) return desc;
    else raise(e_TypeUnknown, cat(
        "C++ type ", get_demangled_name(t), " doesn't have an AYU_DESCRIBE"
    ));
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

StaticString get_description_name (const Description* desc) {
    return desc->name_offset
        ? ((NameDcr<Mu>*)((char*)desc + desc->name_offset))->f()
        : !desc->name.empty() ? desc->name
        : StaticString(desc->cpp_type->name());
}

UniqueString get_demangled_name (const std::type_info& t) noexcept {
#if __has_include(<cxxabi.h>)
    int status;
    char* demangled = abi::__cxa_demangle(t.name(), nullptr, nullptr, &status);
    if (status != 0) return cat("?(Failed to demangle ", t.name(), ')');
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
