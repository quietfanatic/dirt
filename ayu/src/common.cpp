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
        mess_cache = cat(
            '[', derived.type.name(), ' ', item_to_string(derived), ']', '\0'
        );
    }
    else mess_cache = "?(Couldn't downcast error data)\0";
    return mess_cache.data();
}

void unrecoverable_exception (std::exception& e, Str when) {
    warn_utf8(cat(
        "ERROR: Unrecoverable exception ", when,
        ":\n       ", e.what(), '\n'
    ));
    std::terminate();
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Error)

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
