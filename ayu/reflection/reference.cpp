#include "reference.h"

#include "../../iri/iri.h"
#include "../traversal/compound.h"
#include "../traversal/location.h"
#include "../traversal/scan.h"
#include "../traversal/to-tree.h"
#include "describe.h"

namespace ayu {
using namespace in;

[[gnu::cold]]
void Reference::raise_WriteReadonly () const {
    try {
        SharedLocation here = reference_to_location(*this);
        raise(e_ReferenceReadonly, cat(
            "Can't write to readonly Reference of type ", type().name(),
            " at ", item_to_string(&here)
        ));
    }
    catch (std::exception& e) {
        raise(e_ReferenceReadonly, cat(
            "Can't write to readonly Reference of type ", type().name(),
            " at (!exception thrown while getting location of Reference: ", e.what()
        ));
    }
}

[[gnu::cold]]
void Reference::raise_Unaddressable () const {
    try {
        SharedLocation here = reference_to_location(*this);
        raise(e_ReferenceUnaddressable, cat(
            "Can't get address of unaddressable Reference of type ", type().name(),
            " at ", item_to_string(&here)
        ));
    }
    catch (std::exception& e) {
        raise(e_ReferenceUnaddressable, cat(
            "Can't get address of unaddressable Reference of type ", type().name(),
            " at (!exception thrown while getting location of Reference: ", e.what()
        ));
    }
}

Reference Reference::operator[] (const AnyString& key) {
    return item_attr(*this, key);
}
Reference Reference::operator[] (usize index) {
    return item_elem(*this, index);
}

} using namespace ayu;

static Tree Reference_to_tree (const Reference& v) {
    if (!v) return Tree(null);
    auto loc = reference_to_location(v);
    auto iri = location_to_iri(loc);
    return Tree(iri.relative_to(current_base_iri()));
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

