#include "common.internal.h"
#include "../iri/iri.h"
#include "../uni/errors.h"
#include "../uni/io.h"
#include "traversal/route.h"
#include "traversal/to-tree.h"

namespace ayu {
using namespace in;

void dump_refs (Slice<Link> rs) {
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

} using namespace ayu;

