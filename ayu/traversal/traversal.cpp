#include "traversal.private.h"
#include "scan.h"

namespace ayu::in {

static void to_reference_parent_addressable (const Traversal&, void*);
static void to_reference_chain (const Traversal&, void*);

 // noexcept because any user code called from here should be confirmed to
 // already work without throwing.
NOINLINE
void Traversal::to_reference (void* r) const noexcept {
    if (caps % AC::Address) {
        new (r) AnyRef(AnyPtr(type, address, caps));
    }
    else if (step == TraversalStep::Start) {
        auto& self = static_cast<const StartTraversal&>(*this);
        new (r) AnyRef(*self.reference);
    }
    else if (parent->caps % AC::Address) {
         // This won't tail call for some reason
        to_reference_parent_addressable(*this, r);
    }
    else to_reference_chain(*this, r);
}

NOINLINE static
void to_reference_parent_addressable (const Traversal& trav, void* r) {
    switch (trav.step) {
        case TraversalStep::Acr: {
            auto& self = static_cast<const AcrTraversal&>(trav);
            expect(self.parent->caps % AC::Address);
            new (r) AnyRef(self.parent->address, self.acr, self.parent->caps);
            return;
        }
        case TraversalStep::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(trav);
            new (r) AnyRef(self.func(*self.parent->address, *self.key));
            return;
        }
        case TraversalStep::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(trav);
            new (r) AnyRef(self.func(*self.parent->address, self.index));
            return;
        }
        case TraversalStep::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(trav);
            auto data = self.func(*self.parent->address);
            auto desc = DescriptionPrivate::get(trav.type);
            data.address = (Mu*)(
                (char*)data.address + self.index * desc->cpp_size
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
    switch (trav.step) {
        case TraversalStep::Acr: {
            auto& self = static_cast<const AcrTraversal&>(trav);
            new (r) AnyRef(parent_ref.host, new ChainAcr(
                parent_ref.acr(), self.acr, trav.caps
            ));
            return;
        }
        case TraversalStep::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(trav);
            new (r) AnyRef(parent_ref.host, new ChainAttrFuncAcr(
                parent_ref.acr(), self.func, *self.key, trav.caps
            ));
            return;
        }
        case TraversalStep::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(trav);
            new (r) AnyRef(parent_ref.host, new ChainElemFuncAcr(
                parent_ref.acr(), self.func, self.index, trav.caps
            ));
            return;
        }
        case TraversalStep::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(trav);
            new (r) AnyRef(parent_ref.host, new ChainDataFuncAcr(
                parent_ref.acr(), self.func, self.index, trav.caps
            ));
            return;
        }
        default: never();
    }
}

[[gnu::cold]] NOINLINE
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
