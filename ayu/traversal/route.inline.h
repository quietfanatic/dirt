#pragma once

#include "../../uni/indestructible.h"

namespace ayu {
namespace in {

 // ResourceRoute is extern to break a cyclic dependency.
struct ReferenceRoute : Route {
    AnyRef reference;
    ReferenceRoute (AnyRef ref) :
        Route(RF::Reference), reference(move(ref))
    { expect(reference); }
};
struct ChildRoute : Route {
    SharedRoute parent;
    ChildRoute (RouteForm f, SharedRoute p) : Route(f), parent(move(p))
    { expect(parent); }
};
struct KeyRoute : ChildRoute {
    AnyString key;
    KeyRoute (SharedRoute p, AnyString k) :
        ChildRoute(RF::Key, move(p)), key(move(k))
    { }
};
struct IndexRoute : ChildRoute {
    u32 index;
    IndexRoute (SharedRoute p, u32 i) :
        ChildRoute(RF::Index, move(p)), index(i)
    { }
};

};

inline SharedRoute::SharedRoute (const AnyRef& ref) noexcept :
    data(new in::ReferenceRoute(ref))
{ }
inline SharedRoute::SharedRoute (SharedRoute p, AnyString k) noexcept :
    data(new in::KeyRoute(move(p), move(k)))
{ }
inline SharedRoute::SharedRoute (SharedRoute p, u32 i) noexcept :
    data(new in::IndexRoute(move(p), i))
{ }

inline const AnyRef* Route::reference () const noexcept {
    if (form != RF::Reference) return null;
    else return &static_cast<const in::ReferenceRoute*>(this)->reference;
}

inline RouteRef Route::parent () const noexcept {
    if (u8(form) < u8(RF::Key)) return {};
    else return static_cast<const in::ChildRoute*>(this)->parent;
}
inline const AnyString* Route::key () const noexcept {
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

inline const IRI& CurrentBase::iri () const noexcept {
    if (!iri_) [[unlikely]] {
        iri_ = route_to_iri(route).chop_fragment();
    }
    return iri_;
}

} // ayu
