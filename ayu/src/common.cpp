#include "../internal/common-internal.h"

#include "../../uni/utf.h"
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

const char* Error::what () const noexcept {
    return (what_cache = cat(code, "; ", details)).c_str();
}
Error::~Error () { }

void raise (ErrorCode code, MoveRef<UniqueString> details) {
    Error e;
    e.code = code;
    e.details = *move(details);
    throw e;
}

[[gnu::cold]]
void unrecoverable_exception (Str when) noexcept {
    try {
        throw std::current_exception();
    } catch (std::exception& e) {
        warn_utf8(cat(
            "ERROR: Unrecoverable exception ", when, ":\n    ",
            get_demangled_name(typeid(e)), ": ", e.what()
        ));
        std::terminate();
    } catch (...) {
        warn_utf8(cat(
            "ERROR: Unrecoverable exception ", "of non-standard type ", when
        ));
        std::terminate();
    }
}

} using namespace ayu;

