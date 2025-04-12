#include "type.h"
#include <typeindex>
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif
#include "../../uni/hash.h"
#include "describe.h"
#include "description.private.h"

namespace ayu {
namespace in {

struct TypeRegistry {
    UniqueArray<Hashed<const DescriptionPrivate*>> by_name;
    bool initted = false;
};

 // TODO: constinit
static TypeRegistry& registry () {
    static TypeRegistry r;
    return r;
}

static
StaticString get_type_name_cached (const DescriptionPrivate* desc) {
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
        auto n = Type(p.value).name();
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
            auto a = reinterpret_cast<const Hashed<const DescriptionPrivate*>*>(aa);
            auto b = reinterpret_cast<const Hashed<const DescriptionPrivate*>*>(bb);
            if (a->hash != b->hash) [[likely]] {
                 // can't subtract here, it'll overflow
                return a->hash < b->hash ? -1 : 1;
            }
            auto an = get_type_name_cached(a->value);
            auto bn = get_type_name_cached(b->value);
            if (an.size() == bn.size()) {
                return std::memcmp(an.data(), bn.data(), an.size());
            }
            else return int(an.size() - bn.size());
        }
    );
    plog("init types end");
}

void register_description (const void* desc) noexcept {
    require(!registry().initted);
    registry().by_name.emplace_back(0, reinterpret_cast<const DescriptionPrivate*>(desc));
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

[[noreturn, gnu::cold]]
static void raise_TypeCantDefaultConstruct (Type t) {
    raise(e_TypeCantDefaultConstruct, cat(
        "Type ", t.name(), " has no default constructor."
    ));
}

[[noreturn, gnu::cold]]
static void raise_TypeCantDestroy (Type t) {
    raise(e_TypeCantDestroy, cat(
        "Type ", t.name(), " has no destructor."
    ));
}

[[noreturn, gnu::cold]]
static void raise_TypeCantCast (Type from, Type to) {
    raise(e_TypeCantCast, cat(
        "Can't cast from ", from.name(), " to ", to.name()
    ));
}

} using namespace in;

 // in current gcc, this optimization interferes with conditional moves
[[gnu::optimize("-fno-thread-jumps")]]
Type::Type (Str name, bool readonly) {
    auto& r = registry();
    if (!r.initted) [[unlikely]] init_names();
    if (!name) {
        ptr = null; return;
    }
    auto h = uni::hash(name);
    u32 bottom = 0;
    u32 top = r.by_name.size();
    while (bottom != top) {
        u32 mid = (top + bottom) / 2;
        auto& e = r.by_name[mid];
        if (e.hash == h) [[unlikely]] {
            Str n = get_type_name_cached(e.value);
            if (n == name) [[likely]] {
                ptr = e.value;
                if (readonly) data |= 1;
                return;
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
    raise(e_TypeNotFound, cat(
        "Did not find type named ", name
    ));
}

StaticString Type::name () const noexcept {
    if (!*this) return "";
    auto desc = description();
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

usize Type::cpp_size () const {
    return description()->cpp_size;
}
usize Type::cpp_align () const {
    return description()->cpp_align;
}

void Type::default_construct (void* target) const {
    auto desc = description();
    if (!desc->default_construct) raise_TypeCantDefaultConstruct(*this);
     // Don't allow constructing objects that can't be destroyed
    if (!desc->destroy) raise_TypeCantDestroy(*this);
    desc->default_construct(target);
}

void Type::destroy (Mu* p) const {
    auto desc = description();
    if (!desc->destroy) raise_TypeCantDestroy(*this);
    desc->destroy(p);
}

void* Type::allocate () const noexcept {
    auto desc = description();
    void* r = operator new(
        desc->cpp_size, std::align_val_t(desc->cpp_align), std::nothrow
    );
    return expect(r);
}

void Type::deallocate (void* p) const noexcept {
    auto desc = description();
    operator delete(p, desc->cpp_size, std::align_val_t(desc->cpp_align));
}

Mu* Type::default_new () const {
    auto desc = description();
     // Throw before allocating
    if (!desc->default_construct) raise_TypeCantDefaultConstruct(*this);
    if (!desc->destroy) raise_TypeCantDestroy(*this);
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

    auto desc = description();
    if (auto delegate = desc->delegate_acr())
    if (AnyPtr a = delegate->address(*p))
    if (Mu* b = a.type.try_upcast_to(to, a.address))
        return b;

    if (!desc->keys_acr())
    if (auto attrs = desc->attrs())
    for (size_t i = 0; i < attrs->n_attrs; i++) {
        auto acr = attrs->attr(i)->acr();
        if (!!(acr->attr_flags & AttrFlags::Include))
        if (AnyPtr a = acr->address(*p))
        if (Mu* b = a.type.try_upcast_to(to, a.address))
            return b;
    }

    if (!desc->length_acr())
    if (auto elems = desc->elems())
    for (size_t i = 0; i < elems->n_elems; i++) {
        auto acr = elems->elem(i)->acr();
        if (!!(acr->attr_flags & AttrFlags::Include))
        if (AnyPtr a = acr->address(*p))
        if (Mu* b = a.type.try_upcast_to(to, a.address))
            return b;
    }
    return null;
}
Mu* Type::upcast_to (Type to, Mu* p) const {
    if (!p) return null;
    if (Mu* r = try_upcast_to(to, p)) return r;
    else raise_TypeCantCast(*this, to);
}

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

// Testing of Type will be done in anyval.cpp
