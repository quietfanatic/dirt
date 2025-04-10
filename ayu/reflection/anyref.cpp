#include "anyref.h"
#include "../../iri/iri.h"
#include "../traversal/compound.h"
#include "../traversal/route.h"
#include "../traversal/scan.h"
#include "../traversal/to-tree.h"
#include "describe.h"

namespace ayu {
using namespace in;

[[gnu::cold]]
void AnyRef::raise_WriteReadonly () const {
    try {
        SharedRoute here = reference_to_route(*this);
        raise(e_WriteReadonly, cat(
            "Can't write to readonly reference of type ", type().name(),
            " at ", item_to_string(&here)
        ));
    }
    catch (std::exception& e) {
        raise(e_WriteReadonly, cat(
            "Can't write to readonly reference of type ", type().name(),
            " at (!exception thrown while getting route of AnyRef: ", e.what()
        ));
    }
}

[[gnu::cold]]
void AnyRef::raise_Unaddressable () const {
    try {
        SharedRoute here = reference_to_route(*this);
        raise(e_ReferenceUnaddressable, cat(
            "Can't get address of unaddressable reference of type ", type().name(),
            " at ", item_to_string(&here)
        ));
    }
    catch (std::exception& e) {
        raise(e_ReferenceUnaddressable, cat(
            "Can't get address of unaddressable reference of type ", type().name(),
            " at (!exception thrown while getting route of AnyRef: ", e.what()
        ));
    }
}

AnyRef AnyRef::operator[] (const AnyString& key) const {
    return item_attr(*this, key);
}
AnyRef AnyRef::operator[] (u32 index) const {
    return item_elem(*this, index);
}

} using namespace ayu;

static Tree AnyRef_to_tree (const AnyRef& v) {
    if (!v) return Tree(null);
    auto rt = reference_to_route(v);
    auto iri = route_to_iri(rt);
    return Tree(iri.relative_to(current_base_iri()));
}
static void AnyRef_from_tree (AnyRef& v, const Tree& tree) {
    switch (tree.form) {
        case Form::Null: break;
        case Form::String: if (!Str(tree)) raise(e_General,
            "Cannot deserialize AnyRef from empty IRI.  To make the null AnyRef, use null."
        ); break;
        default: raise_FromTreeFormRejected(Type::For<AnyRef>(), tree.form);
    }
    v = AnyRef();
}
static void AnyRef_swizzle (AnyRef& v, const Tree& tree) {
    if (tree.form == Form::Null) return;
    auto iri = IRI(Str(tree), current_base_iri());
    auto rt = route_from_iri(iri);
    v = reference_from_route(rt);
}

AYU_DESCRIBE(ayu::AnyRef,
     // Can't use delegate with &reference_to_route, because the call to
     // reference_to_route will trigger a scan, which will try to follow the
     // delegation by calling reference_to_route, ad inifinitum.  This does mean
     // you can't have an AnyRef pointing to a Route that is actually an AnyRef.
     // Which...well, if you get to the point where you're trying to do that,
     // you should probably refactor anyway, after seeing a doctor.
    to_tree(&AnyRef_to_tree),
    from_tree(&AnyRef_from_tree),
    swizzle(&AnyRef_swizzle)
)

