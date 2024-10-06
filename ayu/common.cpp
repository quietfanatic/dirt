#include "common.internal.h"

#include "../uni/io.h"
#include "traversal/to-tree.h"

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

} using namespace ayu;

