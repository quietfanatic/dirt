#include "link.h"
#include "../../iri/iri.h"
#include "../traversal/compound.h"
#include "../traversal/route.h"
#include "../traversal/scan.h"
#include "../traversal/to-tree.h"
#include "describe.h"

namespace ayu {
using namespace in;

[[gnu::cold]]
void Link::raise_access_denied (AccessCaps mode) const {
    const char* code;
    StaticString mess;
    if ((mode & ~caps()) % AC::Write) {
        code = e_WriteReadonly;
        mess = "Can't write to readonly link of type ";
    }
    else if ((mode & ~caps()) % AC::Address) {
        code = e_AddressUnaddressable;
        mess = "Can't get address of unaddressable link of type ";
    }
    else {
        code = e_AccessDenied;
        mess = "Failed to access link of type ";
    }
    try {
        SharedRoute here = link_to_route(*this);
        raise(code, cat(mess, type().name(), " at ", show(&here)));
    }
    catch (std::exception& e) {
        raise(code, cat(
            mess, type().name(),
            " at (exception thrown while getting route of Link: ", e.what(), ")"
        ));
    }
}

Link Link::operator[] (const AnyString& key) const {
    return item_attr(*this, key);
}
Link Link::operator[] (u32 index) const {
    return item_elem(*this, index);
}

} using namespace ayu;

static Tree Link_to_tree (const Link& v) {
    if (!v) return Tree(null);
    auto rt = link_to_route(v);
    auto iri = route_to_iri(rt);
    return Tree(iri.relative_to(current_base_iri()));
}
static bool Link_from_tree (Link& v, const Tree& tree) {
    switch (tree.form) {
        case Form::Null: break;
        case Form::String: if (!Str(tree)) raise(e_General,
            "Cannot deserialize Link from empty IRI.  To make the empty Link, use null."
        ); break;
        default: raise_FromTreeFormRejected(Type::of<Link>(), tree.form);
    }
    v = Link();
    return true;
}
static void Link_swizzle (Link& v, const Tree& tree) {
    if (tree.form == Form::Null) return;
    auto iri = IRI(Str(tree), current_base_iri());
    auto rt = route_from_iri(iri);
    v = link_from_route(rt);
}

AYU_DESCRIBE(ayu::Link,
     // Can't use delegate with &link_to_route, because the call to
     // link_to_route will trigger a scan, which will try to follow the
     // delegation by calling link_to_route, ad inifinitum.  This does mean you
     // can't have a Link pointing to a Route that is actually a Link.  If you
     // get to the point where you're trying to do that, you should probably
     // refactor anyway, after seeing a doctor.
    to_tree(&Link_to_tree),
    from_tree(&Link_from_tree),
    swizzle(&Link_swizzle)
)

