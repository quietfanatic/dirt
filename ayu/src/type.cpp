#include "../type.h"

#include "../describe.h"
#include "descriptors-private.h"

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

void Type::default_construct (void* target) const {
    auto desc = DescriptionPrivate::get(*this);
    if (!desc->default_construct) raise_TypeCantDefaultConstruct(*this);
     // Don't allow constructing objects that can't be destroyed
    if (!desc->destroy) raise_TypeCantDestroy(*this);
    desc->default_construct(target);
}

void Type::destroy (Mu* p) const {
    auto desc = DescriptionPrivate::get(*this);
    if (!desc->destroy) raise_TypeCantDestroy(*this);
    desc->destroy(p);
}

void* Type::allocate () const noexcept {
    auto desc = DescriptionPrivate::get(*this);
    return expect(std::aligned_alloc(desc->cpp_align, desc->cpp_size));
}

void Type::deallocate (void* p) const noexcept {
    std::free(p);
}

Mu* Type::default_new () const {
    auto desc = DescriptionPrivate::get(*this);
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
    auto desc = DescriptionPrivate::get(*this);

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
    else raise_TypeCantCast(*this, to);
}

Mu* Type::try_downcast_to (Type to, Mu* p) const {
    if (!to || !p) return null;
     // Downcasting is unsafe anyway, so allow downcasting from readonly to
     // non-readonly.
    if (this->remove_readonly() == to.remove_readonly()) return p;
    auto desc = DescriptionPrivate::get(to);

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
    else raise_TypeCantCast(*this, to);
}

Mu* Type::try_cast_to (Type to, Mu* p) const {
    if (!p) return p;
    if (Mu* r = try_upcast_to(to, p)) return r;
    else return try_downcast_to(to, p);
}
Mu* Type::cast_to (Type to, Mu* p) const {
    if (!p) return p;
    if (Mu* r = try_cast_to(to, p)) return r;
    else raise_TypeCantCast(*this, to);
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
