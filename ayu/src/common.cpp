#include "../internal/common-internal.h"

#include "../../uni/io.h"
#include "../serialize-to-tree.h"

namespace ayu {
using namespace in;

void dump_refs (Slice<Reference> rs) {
    DiagnosticSerialization _;
    switch (rs.size()) {
        case 0: warn_utf8("[]\n"); break;
        case 1: warn_utf8(item_to_string(rs[0])); break;
        default: {
            UniqueString r = "[";
            r.append(item_to_string(rs[0]));
            for (usize i = 1; i < rs.size(); i++) {
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

