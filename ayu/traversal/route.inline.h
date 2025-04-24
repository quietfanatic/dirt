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
struct KeyRoute : Route {
    SharedRoute parent;
    AnyString key;
    KeyRoute (SharedRoute p, AnyString k) :
        Route(RF::Key), parent(move(p)), key(move(k))
    { expect(parent); }
};
struct IndexRoute : Route {
    SharedRoute parent;
    u32 index;
    IndexRoute (SharedRoute p, u32 i) :
        Route(RF::Index), parent(move(p)), index(i)
    { expect(parent); }
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
    switch (form) {
        case RF::Reference:
            return &static_cast<const in::ReferenceRoute*>(this)->reference;
        default: return null;
    }
}

inline RouteRef Route::parent () const noexcept {
    switch (form) {
        case RF::Key: return static_cast<const in::KeyRoute*>(this)->parent;
        case RF::Index:
            return static_cast<const in::IndexRoute*>(this)->parent;
        default: return {};
    }
}
inline const AnyString* Route::key () const noexcept {
    switch (form) {
        case RF::Key: return &static_cast<const in::KeyRoute*>(this)->key;
        default: return null;
    }
}
inline const u32* Route::index () const noexcept {
    switch (form) {
        case RF::Index:
            return &static_cast<const in::IndexRoute*>(this)->index;
        default: return null;
    }
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
