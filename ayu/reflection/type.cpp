#include "type.h"
#include <typeindex>
#include "../../uni/hash.h"
#include "describe.h"
#include "description.private.h"

namespace ayu {
namespace in {

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

struct TypeRegistry {
    UniqueArray<Hashed<const DescriptionPrivate*>> by_name;
    bool initted = false;
};

static constinit TypeRegistry registry;

void register_description (const void* desc) noexcept {
    require(!registry.initted);
    registry.by_name.emplace_back(0, reinterpret_cast<const DescriptionPrivate*>(desc));
}

static
StaticString get_type_name_cached (const DescriptionPrivate* desc) {
    if (desc->flags % DescFlags::NameComputed) {
        return *desc->computed_name.cache;
    }
    else if (desc->flags % DescFlags::NameLocal) {
        return StaticString(Str(desc->local_name));
    }
    else return desc->name;
}

NOINLINE static
void init_names () {
    registry.initted = true;
    plog("init types begin");
    for (auto& p : registry.by_name) {
        auto n = Type((const void*)p.value).name();
        require(n);
        p.hash = uni::hash(n);
    }
     // Why we using qsort instead of std::sort, isn't std::sort faster?  Yes,
     // but the reason it's faster is that it generates 10k of code for every
     // callsite, which is overkill for something that'll be called exactly
     // once at init time.  In contrast, qsort is already in the C library, so
     // we get it for free.
    std::qsort(
        registry.by_name.data(), registry.by_name.size(), sizeof(registry.by_name[0]),
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

 // in current gcc, this optimization interferes with conditional moves
[[gnu::optimize("-fno-thread-jumps")]]
const DescriptionHeader* require_type_with_name (Str name) {
    if (!registry.initted) [[unlikely]] init_names();
    auto h = uni::hash(name);
    u32 bottom = 0;
    u32 top = registry.by_name.size();
    while (bottom != top) {
        u32 mid = (top + bottom) / 2;
        auto& e = registry.by_name[mid];
        if (e.hash == h) [[unlikely]] {
            Str n = get_type_name_cached(e.value);
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
    raise(e_TypeNameNotFound, cat("Did not find type named ", name));
}

StaticString DescriptionHeader::get_name () const noexcept {
    if (flags % DescFlags::NameComputed) {
        auto cache = computed_name.cache;
        if (!*cache) {
            AnyString s = computed_name.f();
            *cache = StaticString(s);
            s.impl = {};
        }
        return *cache;
    }
    else if (flags % DescFlags::NameLocal) {
        return StaticString(Str(local_name));
    }
    else return name;
}

} using namespace in;

void dynamic_default_construct (Type t, void* p) {
    auto desc = DescriptionPrivate::get(t);
    if (!desc->default_construct) raise_TypeCantDefaultConstruct(t);
    if (!desc->destroy) raise_TypeCantDestroy(t);
    desc->default_construct(p);
}

void dynamic_default_construct_without_destructor (Type t, void* p) {
    auto desc = DescriptionPrivate::get(t);
    if (!desc->default_construct) raise_TypeCantDefaultConstruct(t);
    desc->default_construct(p);
}

void dynamic_destroy (Type t, Mu* p) {
    auto desc = DescriptionPrivate::get(t);
    if (!desc->destroy) raise_TypeCantDestroy(t);
    desc->destroy(p);
}

Mu* dynamic_default_new (Type t) {
    auto desc = DescriptionPrivate::get(t);
     // Throw before allocating
    if (!desc->default_construct) raise_TypeCantDefaultConstruct(t);
    if (!desc->destroy) raise_TypeCantDestroy(t);
    void* p = dynamic_allocate(t);
    desc->default_construct(p);
    return (Mu*)p;
}

void dynamic_delete (Type t, Mu* p) {
    dynamic_destroy(t, p);
    dynamic_deallocate(t, p);
}

Mu* dynamic_try_upcast (Type from, Type to, Mu* p) {
    if (!p) return null;
    if (from == to) return p;

    auto desc = DescriptionPrivate::get(from);
    if (auto delegate = desc->delegate_acr())
    if (AnyPtr a = delegate->address(*p))
    if (Mu* b = dynamic_try_upcast(a.type(), to, a.address))
        return b;

    if (!desc->keys_acr())
    if (auto attrs = desc->attrs())
    for (size_t i = 0; i < attrs->n_attrs; i++) {
        auto acr = attrs->attr(i)->acr();
        if (acr->attr_flags % AttrFlags::Castable)
        if (AnyPtr a = acr->address(*p))
        if (Mu* b = dynamic_try_upcast(a.type(), to, a.address))
            return b;
    }

    if (!desc->length_acr())
    if (auto elems = desc->elems())
    for (size_t i = 0; i < elems->n_elems; i++) {
        auto acr = elems->elem(i)->acr();
        if (acr->attr_flags % AttrFlags::Castable)
        if (AnyPtr a = acr->address(*p))
        if (Mu* b = dynamic_try_upcast(a.type(), to, a.address))
            return b;
    }
    return null;
}

Mu* dynamic_upcast (Type from, Type to, Mu* p) {
    if (!p) return null;
    if (Mu* r = dynamic_try_upcast(from, to, p)) return r;
    else raise_TypeCantCast(from, to);
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Type,
    to_tree([](const Type& v){
        if (v) return Tree(v.name());
        else return Tree(null);
    }),
    from_tree([](Type& v, const Tree& t){
        if (t.form == Form::Null) v = Type();
        else v = Type(Str(t));
    })
)

// Testing of Type will be done in anyval.cpp
