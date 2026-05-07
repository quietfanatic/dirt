#pragma once

#include "../../uni/indestructible.h"

namespace ayu {
namespace in {

struct ResourceRoute : Route {
    SharedResource resource;
    ResourceRoute (SharedResource res) :
        Route(RF::Resource), resource(move(res))
    { expect(resource); }
};
struct LinkRoute : Route {
    Link link;
    LinkRoute (Link l) :
        Route(RF::Link), link(move(l))
    { expect(link); }
};
struct ChildRoute : Route {
    SharedRoute parent;
    ChildRoute (RouteForm f, SharedRoute p) : Route(f), parent(move(p))
    { expect(parent); }
};
struct KeyRoute : ChildRoute {
    SharedString key;
    KeyRoute (SharedRoute p, SharedString k) :
        ChildRoute(RF::Key, move(p)), key(move(k))
    { }
};
struct IndexRoute : ChildRoute {
    u32 index;
    IndexRoute (SharedRoute p, u32 i) :
        ChildRoute(RF::Index, move(p)), index(i)
    { }
};

} // in

inline SharedRoute::SharedRoute (ResourceRef res) noexcept :
    data(new in::ResourceRoute(res))
{ }
inline SharedRoute::SharedRoute (const Link& l) noexcept :
    data(new in::LinkRoute(l))
{ }
inline SharedRoute::SharedRoute (SharedRoute p, SharedString k) noexcept :
    data(new in::KeyRoute(move(p), move(k)))
{ }
inline SharedRoute::SharedRoute (SharedRoute p, u32 i) noexcept :
    data(new in::IndexRoute(move(p), i))
{ }

 // Incorrent warning here, "warning: ‘<anonymous>’ may be used uninitialized"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
inline ResourceRef Route::resource () const noexcept {
    if (form != RF::Resource) return {};
    else return static_cast<const in::ResourceRoute*>(this)->resource;
}
#pragma GCC diagnostic pop
inline const Link* Route::link () const noexcept {
    if (form != RF::Link) return null;
    else return &static_cast<const in::LinkRoute*>(this)->link;
}
inline RouteRef Route::parent () const noexcept {
    if (u8(form) < u8(RF::Key)) return {};
    else return static_cast<const in::ChildRoute*>(this)->parent;
}
inline const SharedString* Route::key () const noexcept {
    if (form != RF::Key) return null;
    else return &static_cast<const in::KeyRoute*>(this)->key;
}
inline const u32* Route::index () const noexcept {
    if (form != RF::Index) return null;
    else return &static_cast<const in::IndexRoute*>(this)->index;
}

inline RouteRef Route::root () const noexcept {
    auto r = RouteRef(this);
    while (r->parent()) r = r->parent();
    return r;
}

inline const IRI& current_base_iri () noexcept {
    static constexpr IRI empty;
    if (!current_base) return empty;
    switch (current_base->form) {
        case RF::Resource: return current_base->resource()->name();
        case RF::Link: return anonymous_iri;
        default: never();
    }
}

} // ayu
