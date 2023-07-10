#include "../reference.h"

#include "accessors-private.h"
#include "descriptors-private.h"
#include "../describe.h"
#include "../dynamic.h"
#include "../resource.h"
#include "../scan.h"
#include "../serialize.h"

namespace ayu {
using namespace in;

void Reference::require_writeable () const {
    if (readonly()) {
        throw WriteReadonlyReference(reference_to_location(*this), type());
    }
}

Mu* Reference::require_address () const {
    if (!*this) return null;
    if (auto a = address()) return a;
    else {
        throw UnaddressableReference(reference_to_location(*this), type());
    }
}

Reference Reference::chain (const Accessor* o_acr) const {
    if (auto a = address()) {
        return Reference(Pointer(type(), a), o_acr);
    }
    else {
        return Reference(host, new ChainAcr(acr, o_acr));
    }
}

Reference Reference::chain_attr_func (
    Reference(* f )(Mu&, AnyString), AnyString k
) const {
    if (auto a = address()) {
        auto r = f(*a, k);
        if (r) return r;
        else throw AttrNotFound(move(k));
    }
    else {
         // Extra read just to check if the func returns null Reference.
         // If we're here, we're already on a fairly worst-case performance
         // scenario, so one more check isn't gonna make much difference.
        read([&](const Mu& v){
            Reference ref = f(const_cast<Mu&>(v), k);
            if (!ref) throw AttrNotFound(move(k));
        });
        return Reference(host, new ChainAcr(acr, new AttrFuncAcr(f, move(k))));
    }
}

Reference Reference::chain_elem_func (
    Reference(* f )(Mu&, size_t), size_t i
) const {
    if (auto a = address()) {
        auto r = f(*a, i);
        if (r) return r;
        else throw ElemNotFound(i);
    }
    else {
        read([&](const Mu& v){
            Reference ref = f(const_cast<Mu&>(v), i);
            if (!ref) throw ElemNotFound(i);
        });
        return Reference(host, new ChainAcr(acr, new ElemFuncAcr(f, i)));
    }
}

Reference Reference::operator[] (AnyString key) const {
    return item_attr(*this, move(key));
}
Reference Reference::operator[] (usize i) const {
    return item_elem(*this, i);
}

} using namespace ayu;

static Tree Reference_to_tree (const Reference& v) {
    if (!v) return Tree(null);
    Location loc = reference_to_location(v);
    return Tree(location_iri_to_relative_iri(loc.as_iri()));
}
static void Reference_from_tree (Reference& v, const Tree& t) {
    switch (t.form) {
        case NULLFORM: case STRING: break;
        default: throw InvalidForm(t.form);
    }
    v = Reference();
}
static void Reference_swizzle (Reference& v, const Tree& t) {
    if (t.form == NULLFORM) return;
    auto loc = Location(location_iri_from_relative_iri(Str(t)));
    v = reference_from_location(loc);
}

AYU_DESCRIBE(ayu::Reference,
     // Can't use delegate with &reference_to_location, because the call to
     // reference_to_location will trigger a scan, which will try to follow the
     // delegation by calling reference_to_location, ad inifinitum.  This does
     // mean you can't have a Reference pointing to a Location that is actually
     // a Reference.  Which...well, if you get to the point where you're trying
     // to do that, you should probably refactor anyway, after seeing a doctor.
    to_tree(&Reference_to_tree),
    from_tree(&Reference_from_tree),
    swizzle(&Reference_swizzle)
)

AYU_DESCRIBE(ayu::ReferenceError,
    elems(
        elem(base<Error>(), include),
        elem(&ReferenceError::location),
        elem(&ReferenceError::type)
    )
)
AYU_DESCRIBE(ayu::WriteReadonlyReference,
    elems(elem(base<ReferenceError>(), include))
)
AYU_DESCRIBE(ayu::UnaddressableReference,
    elems(elem(base<ReferenceError>(), include))
)
