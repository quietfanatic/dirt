#include "traversal.private.h"

namespace ayu::in {

static void to_reference_parent_addressable (const Traversal&, void*);
static void to_reference_chain (const Traversal&, void*);
static void to_route_start_ref (const Traversal&, void*);
static void to_route_chain (const Traversal&, void*);

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
        case TraversalOp::Delegate:
        case TraversalOp::Attr:
        case TraversalOp::Elem: {
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
            auto desc = DescriptionPrivate::get(data.type);
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
    switch (trav.op) {
        case TraversalOp::Attr: case TraversalOp::Elem:
        case TraversalOp::Delegate: {
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

NOINLINE
void Traversal::to_route (void* r) const noexcept {
    if (op == TraversalOp::Start) {
        auto& self = static_cast<const StartTraversal&>(*this);
        if (self.route) new (r) SharedRoute(self.route);
        else to_route_start_ref(*this, r);
    }
    else to_route_chain(*this, r);
}

NOINLINE static
void to_route_start_ref (const Traversal& trav, void* r) {
    auto& self = static_cast<const StartTraversal&>(trav);
     // This * took a half a day of debugging to add. :(
    new (r) SharedRoute(*self.reference);
}

NOINLINE static
void to_route_chain (const Traversal& trav, void* r) {
    SharedRoute parent_rt;
    trav.parent->to_route(&parent_rt);
    switch (trav.op) {
        case TraversalOp::Delegate: {
            new (r) SharedRoute(move(parent_rt));
            return;
        }
        case TraversalOp::Attr: {
            auto& self = static_cast<const AttrTraversal&>(trav);
            if (!!(self.acr->attr_flags & AttrFlags::Include)) {
                 // Collapse route for an included attribute.
                new (r) SharedRoute(move(parent_rt));
            }
            else {
                new (r) SharedRoute(move(parent_rt), *self.key);
            }
            return;
        }
        case TraversalOp::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(trav);
            new (r) SharedRoute(move(parent_rt), *self.key);
            return;
        }
         // These three branches can technically be merged, hopefully the
         // compiler does so.
        case TraversalOp::Elem: {
            auto& self = static_cast<const ElemTraversal&>(trav);
            new (r) SharedRoute(move(parent_rt), self.index);
            return;
        }
        case TraversalOp::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(trav);
            new (r) SharedRoute(move(parent_rt), self.index);
            return;
        }
        case TraversalOp::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(trav);
            new (r) SharedRoute(move(parent_rt), self.index);
            return;
        }
        default: never();
    }
}

} // ayu::in
