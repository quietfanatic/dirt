#include "common.internal.h"
#include "../iri/iri.h"
#include "../uni/io.h"
#include "traversal/route.h"
#include "traversal/to-tree.h"

namespace ayu {
using namespace in;

void dump_refs (Slice<AnyRef> rs) {
    switch (rs.size()) {
        case 0: warn_utf8("[]\n"); break;
        case 1: {
            warn_utf8(cat(show(rs[0]), "\n"));
            break;
        }
        default: {
            UniqueString r = "[";
            r.append(show(rs[0]));
            for (u32 i = 1; i < rs.size(); i++) {
                r.push_back(' ');
                r.append(show(rs[i]));
            }
            r.append("]\n");
            warn_utf8(r);
            break;
        }
    }
}

void in::rethrow_with_route (RouteRef rt) {
    try { throw; }
    catch (Error& e) {
        if (!e.get_tag("ayu::route")) {
            UniqueString spec = rt ?
                route_to_iri(rt).spec() : "!(Could not find route of error)";
            e.add_tag("ayu::route", spec);
        }
        throw e;
    }
    catch (std::exception& ex) {
        Error e;
        e.code = e_External;
        {
            e.details = cat(
#if defined(__GXX_RTTI) || defined(_CPPRTTI)
                get_demangled_name(typeid(ex)), ": ", ex.what()
#else
                "(unknown error type): ", ex.what()
#endif
            );
        }
        e.add_tag("ayu::route", route_to_iri(rt).spec());
        e.external = std::current_exception();
        throw e;
    }
}

} using namespace ayu;

