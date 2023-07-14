#include "../type.h"

#include <cstdlib>
#include <iostream>
#include <typeindex>
#include <string>

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

#include "../internal/descriptors-internal.h"
#include "../describe.h"
#include "descriptors-private.h"

namespace ayu {

StaticString Type::name () const noexcept {
    auto desc = in::DescriptionPrivate::get(*this);
    if (!desc) return "";
    return in::get_description_name(desc);
}

const std::type_info& Type::cpp_type () const noexcept {
    auto desc = in::DescriptionPrivate::get(*this);
    return *desc->cpp_type;
}

usize Type::cpp_size () const noexcept {
    auto desc = in::DescriptionPrivate::get(*this);
    return desc->cpp_size;
}
usize Type::cpp_align () const noexcept {
    auto desc = in::DescriptionPrivate::get(*this);
    return desc->cpp_align;
}

void Type::default_construct (void* target) const {
    auto desc = in::DescriptionPrivate::get(*this);
    if (!desc->default_construct) throw CannotDefaultConstruct(*this);
     // Don't allow constructing objects that can't be destroyed
    if (!desc->destroy) throw CannotDestroy(*this);
    desc->default_construct(target);
}

void Type::destroy (Mu* p) const {
    auto desc = in::DescriptionPrivate::get(*this);
    if (!desc->destroy) throw CannotDestroy(*this);
    desc->destroy(p);
}

void* Type::allocate () const noexcept {
    auto desc = in::DescriptionPrivate::get(*this);
    return expect(std::aligned_alloc(desc->cpp_align, desc->cpp_size));
}

void Type::deallocate (void* p) const noexcept {
    std::free(p);
}

Mu* Type::default_new () const {
    auto desc = in::DescriptionPrivate::get(*this);
     // Throw before allocating
    if (!desc->default_construct) throw CannotDefaultConstruct(*this);
    if (!desc->destroy) throw CannotDestroy(*this);
    void* p = allocate();
    desc->default_construct(p);
    return (Mu*)p;
}

void Type::delete_ (Mu* p) const {
    destroy(p);
    deallocate(p);
}

Mu* Type::try_upcast_to (Type to, Mu* p) const {
    if (!to || !p) return null;
    if (*this == to.remove_readonly()) return p;
    auto desc = in::DescriptionPrivate::get(*this);

    if (auto delegate = desc->delegate_acr())
    if (Mu* a = delegate->address(*p))
    if (Mu* b = delegate->type(p).try_upcast_to(to, a))
        return b;

    if (auto attrs = desc->attrs())
    for (size_t i = 0; i < attrs->n_attrs; i++) {
        auto acr = attrs->attr(i)->acr();
        if (Mu* a = acr->address(*p))
        if (Mu* b = acr->type(p).try_upcast_to(to, a))
            return b;
    }

    if (auto elems = desc->elems())
    for (size_t i = 0; i < elems->n_elems; i++) {
        auto acr = elems->elem(i)->acr();
        if (Mu* a = acr->address(*p))
        if (Mu* b = acr->type(p).try_upcast_to(to, a))
            return b;
    }
    return null;
}
Mu* Type::upcast_to (Type to, Mu* p) const {
    if (!p) return null;
    if (Mu* r = try_upcast_to(to, p)) return r;
    else throw CannotCoerce(*this, to);
}

Mu* Type::try_downcast_to (Type to, Mu* p) const {
    if (!to || !p) return null;
     // Downcasting is unsafe anyway, so allow downcasting from readonly to
     // non-readonly.
    if (this->remove_readonly() == to.remove_readonly()) return p;
    auto desc = in::DescriptionPrivate::get(to);

     // It's okay to pass null to ->type() because the only accessor that
     // actually checks it doesn't have an inverse_address.
    if (auto delegate = desc->delegate_acr())
    if (delegate->vt->inverse_address)
    if (Mu* a = try_downcast_to(delegate->type(null), p))
    if (Mu* b = delegate->inverse_address(*a))
        return b;

    if (auto attrs = desc->attrs())
    for (size_t i = 0; i < attrs->n_attrs; i++) {
        auto acr = attrs->attr(i)->acr();
        if (acr->vt->inverse_address)
        if (Mu* a = try_downcast_to(acr->type(null), p))
        if (Mu* b = acr->inverse_address(*a))
            return b;
    }

    if (auto elems = desc->elems())
    for (size_t i = 0; i < elems->n_elems; i++) {
        auto acr = elems->elem(i)->acr();
        if (acr->vt->inverse_address)
        if (Mu* a = try_downcast_to(acr->type(null), p))
        if (Mu* b = acr->inverse_address(*a))
            return b;
    }
    return null;
}
Mu* Type::downcast_to (Type to, Mu* p) const {
    if (!p) return p;
    if (Mu* r = try_downcast_to(to, p)) return r;
    else throw CannotCoerce(*this, to);
}

Mu* Type::try_cast_to (Type to, Mu* p) const {
    if (!p) return p;
    if (Mu* r = try_upcast_to(to, p)) return r;
    else return try_downcast_to(to, p);
}
Mu* Type::cast_to (Type to, Mu* p) const {
    if (!p) return p;
    if (Mu* r = try_cast_to(to, p)) return r;
    else throw CannotCoerce(*this, to);
}

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

const Description* register_description (const Description* desc) {
    if (registry().initted) {
        throw GenericError("register_description called after init time");
    }
    auto [p, e] = registry().by_cpp_type.emplace(*desc->cpp_type, desc);
    return p->second;
}

const Description* get_description_for_type_info (const std::type_info& t) {
    auto& ds = registry().by_cpp_type;
    auto iter = ds.find(t);
    if (iter != ds.end()) return iter->second;
    else return null;
}
const Description* need_description_for_type_info (const std::type_info& t) {
    auto desc = get_description_for_type_info(t);
    if (desc) return desc;
    else throw UnknownType(t);
}

const Description* get_description_for_name (Str name) {
    init_names();
    auto& ds = registry().by_name;
    auto iter = ds.find(name);
    if (iter != ds.end()) return iter->second;
    else return null;
}
const Description* need_description_for_name (Str name) {
    auto desc = get_description_for_name(name);
    if (desc) return desc;
    else throw TypeNotFound(name);
}

StaticString get_description_name (const Description* desc) {
    return desc->name_offset
        ? ((NameDcr<Mu>*)((char*)desc + desc->name_offset))->f()
        : !desc->name.empty() ? desc->name
        : StaticString(desc->cpp_type->name());
}
bool is_valid_type (const Description* desc) {
    for (auto& p : registry().by_cpp_type) {
        if (p.second == desc) return true;
    }
    return false;
}

UniqueString get_demangled_name (const std::type_info& t) {
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

AYU_DESCRIBE(ayu::Type,
    values(
        value(null, Type())
    ),
    delegate(mixed_funcs<AnyString>(
        [](const Type& v){
            if (v.readonly()) {
                return AnyString(cat(" const", v.name()));
            }
            else return AnyString(v.name());
        },
        [](Type& v, const AnyString& m){
            if (m.substr(m.size() - 6) == " const") {
                v = Type(m.substr(0, m.size() - 6), true);
            }
            else v = Type(m);
        }
    ))
)

AYU_DESCRIBE(ayu::TypeError,
    delegate(base<Error>())
)

AYU_DESCRIBE(ayu::UnknownType,
    elems(
        elem(base<TypeError>(), include),
        elem(value_func<UniqueString>(
            [](const ayu::UnknownType& v){ return get_demangled_name(*v.cpp_type); }
        ))
    )
)

AYU_DESCRIBE(ayu::TypeNotFound,
    elems(
        elem(base<TypeError>(), include),
        elem(&TypeNotFound::name)
    )
)

AYU_DESCRIBE(ayu::CannotDefaultConstruct,
    elems(
        elem(base<TypeError>(), include),
        elem(&CannotDefaultConstruct::type)
    )
)
AYU_DESCRIBE(ayu::CannotDestroy,
    elems(
        elem(base<TypeError>(), include),
        elem(&CannotDestroy::type)
    )
)
AYU_DESCRIBE(ayu::CannotCoerce,
    elems(
        elem(base<TypeError>(), include),
        elem(&CannotCoerce::from),
        elem(&CannotCoerce::to)
    )
)

// Testing of Type will be done in dynamic.cpp
