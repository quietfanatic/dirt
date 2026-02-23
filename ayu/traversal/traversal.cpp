#include "traversal.private.h"
#include "scan.h"

namespace ayu::in {

static void to_link_parent_addressable (const Traversal&, void*) noexcept;
static void to_link_chain (const Traversal&, void*) noexcept;

 // noexcept because any user code called from here should be confirmed to
 // already work without throwing.
NOINLINE
void Traversal::to_link (void* r) const noexcept {
    if (caps % AC::Address) {
        new (r) Link(AnyPtr(type, address, caps));
    }
    else if (step == TraversalStep::Start) {
        auto& self = static_cast<const StartTraversal&>(*this);
        new (r) Link(*self.base_item);
    }
    else if (parent->caps % AC::Address) {
        return to_link_parent_addressable(*this, r);
    }
    else return to_link_chain(*this, r);
}

NOINLINE static
void to_link_parent_addressable (const Traversal& trav, void* r) noexcept {
    switch (trav.step) {
        case TraversalStep::Acr: {
            auto& self = static_cast<const AcrTraversal&>(trav);
            expect(self.parent->caps % AC::Address);
            new (r) Link(self.parent->address, self.acr, self.parent->caps);
            return;
        }
        case TraversalStep::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(trav);
            new (r) Link(self.func(*self.parent->address, *self.key));
            return;
        }
        case TraversalStep::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(trav);
            new (r) Link(self.func(*self.parent->address, self.index));
            return;
        }
        case TraversalStep::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(trav);
            auto data = self.func(*self.parent->address);
            auto desc = DescriptionPrivate::get(trav.type);
            data.address = (Mu*)(
                (char*)data.address + self.index * desc->cpp_size
            );
            new (r) Link(data);
            return;
        }
        default: never();
    }
}

NOINLINE static
void to_link_chain (const Traversal& trav, void* r) noexcept {
    Link parent_link;
    trav.parent->to_link(&parent_link);
    switch (trav.step) {
        case TraversalStep::Acr: {
            auto& self = static_cast<const AcrTraversal&>(trav);
            new (r) Link(parent_link.host, new ChainAcr(
                parent_link.acr(), self.acr, trav.caps
            ));
            return;
        }
        case TraversalStep::ComputedAttr: {
            auto& self = static_cast<const ComputedAttrTraversal&>(trav);
            new (r) Link(parent_link.host, new ChainAttrFuncAcr(
                parent_link.acr(), self.func, *self.key, trav.caps
            ));
            return;
        }
        case TraversalStep::ComputedElem: {
            auto& self = static_cast<const ComputedElemTraversal&>(trav);
            new (r) Link(parent_link.host, new ChainElemFuncAcr(
                parent_link.acr(), self.func, self.index, trav.caps
            ));
            return;
        }
        case TraversalStep::ContiguousElem: {
            auto& self = static_cast<const ContiguousElemTraversal&>(trav);
            new (r) Link(parent_link.host, new ChainDataFuncAcr(
                parent_link.acr(), self.func, self.index, trav.caps
            ));
            return;
        }
        default: never();
    }
}

[[noreturn, gnu::cold]] NOINLINE
void tag_error_with_route_really (Error& e, RouteRef rt) {
    UniqueString tag;
    try { tag = route_to_iri(rt).spec(); }
    catch (...) { }
    if (!tag) tag = "!(Could not find route of error)";
    e.add_tag("ayu::Route", tag);
    throw e;
}

[[noreturn, gnu::cold]] NOINLINE
void tag_error_with_item_really (Error& e, const Link& item) {
    SharedRoute found_rt;
    if (item) try {
        RouteRef base_rt = current_base;
        Link base_item = link_from_route(base_rt);
        scan_links_ignoring_no_links_to_children(
            base_item, base_rt,
            [&](const Link& link, RouteRef rt) {
                return link == item && (found_rt = rt, true);
            }
        );
    }
    catch (...) { }
    Str type = "!(Could not get type of item)";
    try {
        type = item.type().name();
    }
    catch (...) { }
    e.add_tag("ayu::Type", type);
    tag_error_with_route_really(e, found_rt);
}

void tag_error_with_route (RouteRef rt) {
    Error& e = current_error();
    if (e.get_tag("ayu::Route")) throw e;
    tag_error_with_route_really(e, rt);
}

void tag_error_with_item (const Link& item) {
    Error& e = current_error();
    if (e.get_tag("ayu::Route")) throw e;
    tag_error_with_item_really(e, item);
}

void tag_error_with_traversal (const Traversal& trav) {
    Error& e = current_error();
    if (e.get_tag("ayu::Route")) throw e;
    Link item;
    try { trav.to_link(&item); }
    catch (...) { }
    tag_error_with_item_really(e, item);
}

} // ayu::in
