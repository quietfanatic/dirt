#include "../reference.h"

#include "../../iri/iri.h"
#include "../describe.h"
#include "../location.h"
#include "../scan.h"
#include "accessors-private.h"
#include "serialize-compound-private.h"

namespace ayu {
using namespace in;

[[gnu::cold]]
void Reference::raise_WriteReadonly () const {
    Location here = reference_to_location(*this);
    raise(e_ReferenceReadonly, cat(
        "Can't write to readonly Reference of type ", type().name(),
        " at ", item_to_string(&here)
    ));
}

[[gnu::cold]]
void Reference::raise_Unaddressable () const {
    Location here = reference_to_location(*this);
    raise(e_ReferenceUnaddressable, cat(
        "Can't get address of unaddressable Reference of type ", type().name(),
        " at ", item_to_string(&here)
    ));
}

Reference Reference::chain (const Accessor* o_acr) const noexcept {
    if (auto a = address()) {
        return Reference(Pointer(type(), a), o_acr);
    }
    else {
        return Reference(host, new ChainAcr(acr, o_acr));
    }
}

Reference Reference::chain_attr_func (
    AttrFunc<Mu>* attr_func, const AnyString& key
) const {
    if (auto addr = address()) {
        if (auto r = attr_func(*addr, key)) return r;
        else raise_AttrNotFound(type(), key);
    }
    else {
         // Extra read just to check if the func returns null Reference.
         // If we're here, we're already on a fairly worst-case performance
         // scenario, so one more check isn't gonna make much difference.
        read([this, attr_func, &key](const Mu& v){
            Reference ref = attr_func(const_cast<Mu&>(v), key);
            if (!ref) raise_AttrNotFound(type(), key);
        });
        return Reference(host, new ChainAcr(
            acr, new AttrFuncAcr(attr_func, key)
        ));
    }
}

Reference Reference::chain_elem_func (
    ElemFunc<Mu>* elem_func, size_t index
) const {
    if (auto addr = address()) {
        if (auto r = elem_func(*addr, index)) return r;
        else raise_ElemNotFound(type(), index);
    }
    else {
        read([this, elem_func, index](const Mu& v){
            Reference ref = elem_func(const_cast<Mu&>(v), index);
            if (!ref) raise_ElemNotFound(type(), index);
        });
        return Reference(host, new ChainAcr(
            acr, new ElemFuncAcr(elem_func, index)
        ));
    }
}

} using namespace ayu;

static Tree Reference_to_tree (const Reference& v) {
    if (!v) return Tree(null);
    auto loc = reference_to_location(v);
    auto iri = location_to_iri(loc);
    return Tree(iri.spec_relative_to(current_base_iri()));
}
static void Reference_from_tree (Reference& v, const Tree& tree) {
    switch (tree.form) {
        case Form::Null: break;
        case Form::String: if (!Str(tree)) raise(e_General,
            "Cannot make Reference from empty IRI.  To make the null Reference, use null."
        ); break;
        default: raise_FromTreeFormRejected(Type::CppType<Reference>(), tree.form);
    }
    v = Reference();
}
static void Reference_swizzle (Reference& v, const Tree& tree) {
    if (tree.form == Form::Null) return;
    auto iri = IRI(Str(tree), current_base_iri());
    auto loc = location_from_iri(iri);
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

