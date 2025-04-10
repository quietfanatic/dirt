#include "compound.h"

namespace ayu::in {

inline void require_readable_keys (Type t) {
    if (t.remove_readonly() != Type::For<AnyArray<AnyString>>()) {
        raise_KeysTypeInvalid(Type(), t);
    }
}
inline void require_writeable_keys (Type t) {
    if (t != Type::For<AnyArray<AnyString>>()) {
        raise_KeysTypeInvalid(Type(), t);
    }
}

void read_length_acr_cb (u32& len, AnyPtr v, bool);
void write_length_acr_cb (u32& len, AnyPtr v, bool);

inline void read_length_acr (
    u32& len, AnyPtr item, const Accessor* length_acr
) {
    length_acr->read(*item.address, AccessCB(len, read_length_acr_cb));
}

inline void write_length_acr (
    u32& len, AnyPtr item, const Accessor* length_acr
) {
    if (!(length_acr->flags & AcrFlags::Readonly)) {
        length_acr->write(*item.address, AccessCB(len, write_length_acr_cb));
    }
    else {
         // For readonly length, read the length instead and require the given
         // length to equal it.
        u32 expected;
        read_length_acr(expected, item, length_acr);
        if (len != expected) {
            raise_LengthRejected(item.type, expected, expected, len);
        }
    }
}

} // ayu::in
