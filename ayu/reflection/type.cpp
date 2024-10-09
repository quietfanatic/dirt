#include "type.h"

#include "describe.h"
#include "descriptors.private.h"

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

} using namespace in;

void Type::default_construct (this Type t, void* target) {
    auto desc = DescriptionPrivate::get(t);
    if (!desc->default_construct) raise_TypeCantDefaultConstruct(t);
     // Don't allow constructing objects that can't be destroyed
    if (!desc->destroy) raise_TypeCantDestroy(t);
    desc->default_construct(target);
}

void Type::destroy (this Type t, Mu* p) {
    auto desc = DescriptionPrivate::get(t);
    if (!desc->destroy) raise_TypeCantDestroy(t);
    desc->destroy(p);
}

void* Type::allocate (this Type t) noexcept {
    auto desc = DescriptionPrivate::get(t);
    void* r = operator new(
        desc->cpp_size, std::align_val_t(desc->cpp_align), std::nothrow
    );
    return expect(r);
}

void Type::deallocate (this Type t, void* p) noexcept {
    auto desc = DescriptionPrivate::get(t);
    operator delete(p, desc->cpp_size, std::align_val_t(desc->cpp_align));
}

Mu* Type::default_new (this Type t) {
    auto desc = DescriptionPrivate::get(t);
     // Throw before allocating
    if (!desc->default_construct) raise_TypeCantDefaultConstruct(t);
    if (!desc->destroy) raise_TypeCantDestroy(t);
    void* p = t.allocate();
    desc->default_construct(p);
    return (Mu*)p;
}

void Type::delete_ (this Type t, Mu* p) {
    t.destroy(p);
    t.deallocate(p);
}

Mu* Type::try_upcast_to (this Type from, Type to, Mu* p) {
    if (!to || !p) return null;
    if (from == to.remove_readonly()) return p;
    auto desc = DescriptionPrivate::get(from);

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
Mu* Type::upcast_to (this Type from, Type to, Mu* p) {
    if (!p) return null;
    if (Mu* r = from.try_upcast_to(to, p)) return r;
    else raise_TypeCantCast(from, to);
}

using namespace in;
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

// Testing of Type will be done in dynamic.cpp
