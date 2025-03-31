#include "common.internal.h"

#include "../uni/io.h"
#include "traversal/to-tree.h"
#include "traversal/route.h"

namespace ayu {
using namespace in;

void dump_refs (Slice<AnyRef> rs) {
    DiagnosticSerialization _;
    switch (rs.size()) {
        case 0: warn_utf8("[]\n"); break;
        case 1: warn_utf8(cat(item_to_string(rs[0]), "\n")); break;
        default: {
            UniqueString r = "[";
            r.append(item_to_string(rs[0]));
            for (u32 i = 1; i < rs.size(); i++) {
                r.push_back(' ');
                r.append(item_to_string(rs[i]));
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
        if (!e.get_tag("route")) {
            e.add_tag("route", item_to_string(&rt));
        }
        throw e;
    }
    catch (std::exception& ex) {
        Error e;
        e.code = e_External;
        {
            DiagnosticSerialization ds;
            e.details = cat(
                get_demangled_name(typeid(ex)), ": ", ex.what()
            );
        }
        e.add_tag("route", item_to_string(&rt));
        e.external = std::current_exception();
        throw e;
    }
}

} using namespace ayu;

