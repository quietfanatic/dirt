#include "../internal/common-internal.h"

#include <cstdlib>
#include <iostream>

#include "../../uni/utf.h"
#include "../describe.h"
#include "../serialize.h"

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
    auto derived = Pointer(this).try_downcast_to(Type(typeid(*this), true));
    if (derived) {
        DiagnosticSerialization _;
        auto tree = Tree(TreeArray{
            Tree(derived.type.name()),
            item_to_tree(derived)
        });
        mess_cache = cat(tree_to_string(tree, PRETTY), '\0');
    }
    else mess_cache = "?(Couldn't downcast error data)\0";
    return mess_cache.data();
}

//[[gnu::cold]]
void unrecoverable_exception (Str when) {
    auto e = std::current_exception();
    warn_utf8(cat(
        "ERROR: Unrecoverable exception ", when, ":\n",
        item_to_string(&e, PRETTY)
    ));
    std::terminate();
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Error, elems(), attrs())

AYU_DESCRIBE(ayu::GenericError,
    elems(
        elem(base<Error>(), include),
        elem(&GenericError::mess)
    )
)
AYU_DESCRIBE(ayu::IOError,
    elems(
        elem(base<Error>(), include),
        elem(&IOError::filename),
        elem(&IOError::errnum)
    )
)
AYU_DESCRIBE(ayu::OpenFailed,
    elems(
        elem(base<IOError>(), include),
        elem(&OpenFailed::mode)
    )
)
AYU_DESCRIBE(ayu::ReadFailed,
    delegate(base<IOError>())
)
AYU_DESCRIBE(ayu::WriteFailed,
    delegate(base<IOError>())
)
AYU_DESCRIBE(ayu::CloseFailed,
    delegate(base<IOError>())
)
