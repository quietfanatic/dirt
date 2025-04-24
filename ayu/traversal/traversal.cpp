#include "traversal.private.h"
#include "scan.h"

namespace ayu::in {

static void to_reference_parent_addressable (const Traversal&, void*);
static void to_reference_chain (const Traversal&, void*);

 // noexcept because any user code called from here should be confirmed to
 // already work without throwing.
NOINLINE
void Traversal::to_reference (void* r) const noexcept {
    if (addressable) {
        new (r) AnyRef(Type(desc, readonly), address);
    }
    else if (op == TraversalOp::Start) {
        auto& self = static_cast<const StartTraversal&>(*this);
        new (r) AnyRef(*self.reference);
    }
    else if (parent->addressable) {
         // This won't tail call for some reason
        to_reference_parent_addressable(*this, r);
    }
    else to_reference_chain(*this, r);
}

NOINLINE static
void to_reference_parent_addressable (const Traversal& trav, void* r) {
    switch (trav.op) {
        case TraversalOp::Acr: {
            auto& self = static_cast<const AcrTraversal&>(trav);
            auto type = Type(self.parent->desc, self.parent->readonly);
            new (r) AnyRef(AnyPtr(type, self.parent->address), self.acr);
            return;
        }
        case TraversalOp::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(trav);
            new (r) AnyRef(self.func(*self.parent->address, *self.key));
            return;
        }
        case TraversalOp::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(trav);
            new (r) AnyRef(self.func(*self.parent->address, self.index));
            return;
        }
        case TraversalOp::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(trav);
            auto data = self.func(*self.parent->address);
            data.address = (Mu*)(
                (char*)data.address + self.index * trav.desc->cpp_size
            );
            new (r) AnyRef(data);
            return;
        }
        default: never();
    }
}

NOINLINE static
void to_reference_chain (const Traversal& trav, void* r) {
    AnyRef parent_ref;
    trav.parent->to_reference(&parent_ref);
    switch (trav.op) {
        case TraversalOp::Acr: {
            auto& self = static_cast<const AcrTraversal&>(trav);
            new (r) AnyRef(parent_ref.host, new ChainAcr(
                parent_ref.acr, self.acr
            ));
            return;
        }
        case TraversalOp::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(trav);
            new (r) AnyRef(parent_ref.host, new ChainAttrFuncAcr(
                parent_ref.acr, self.func, *self.key
            ));
            return;
        }
        case TraversalOp::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(trav);
            new (r) AnyRef(parent_ref.host, new ChainElemFuncAcr(
                parent_ref.acr, self.func, self.index
            ));
            return;
        }
        case TraversalOp::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(trav);
            new (r) AnyRef(parent_ref.host, new ChainDataFuncAcr(
                parent_ref.acr, self.func, self.index
            ));
            return;
        }
        default: never();
    }
}

void Traversal::wrap_exception () const {
    try { throw; }
    catch (Error& e) {
        if (e.get_tag("ayu::route")) throw;
        AnyRef ref;
        to_reference(&ref);
        rethrow_with_scanned_route(ref);
    }
    catch (...) {
        AnyRef ref;
        to_reference(&ref);
        rethrow_with_scanned_route(ref);
    }
}

NOINLINE
void rethrow_with_scanned_route (const AnyRef& base_item) {
    RouteRef base_rt = current_base->route;
    AnyRef base_ref = reference_from_route(base_rt);
    SharedRoute found_rt;
    try {
        scan_references_ignoring_no_refs_to_children(
            base_ref, base_rt,
            [&](const AnyRef& item, RouteRef rt) {
                return item == base_item && (found_rt = rt, true);
            }
        );
    }
    catch (...) { } // discard exception and leave found_rt blank
    rethrow_with_route(found_rt);
}

} // ayu::in
