#include "compound.h"

namespace ayu::in {

inline const AnyArray<AnyString>& require_readable_keys (Type t, Mu* v) {
    if (t != Type::For<AnyArray<AnyString>>()) {
        raise_KeysTypeInvalid(Type(), t);
    }
    return reinterpret_cast<const AnyArray<AnyString>&>(*v);
}

inline AnyArray<AnyString>& require_writeable_keys (Type t, Mu* v) {
    if (t != Type::For<AnyArray<AnyString>>()) {
        raise_KeysTypeInvalid(Type(), t);
    }
    return reinterpret_cast<AnyArray<AnyString>&>(*v);
}

void read_length_acr_cb (u32& len, Type, Mu*);
void write_length_acr_cb (u32& len, Type, Mu*);

inline void read_length_acr (
    u32& len, Type, Mu* v, const Accessor* length_acr
) {
    length_acr->read(*v, AccessCB(len, read_length_acr_cb));
}

inline void write_length_acr (
    u32& len, Type t, Mu* v, const Accessor* length_acr
) {
    if (length_acr->caps % AC::Write) {
        length_acr->write(*v, AccessCB(len, write_length_acr_cb));
    }
    else {
         // For readonly length, read the length instead and require the given
         // length to equal it.
        u32 expected;
        read_length_acr(expected, t, v, length_acr);
        if (len != expected) {
            raise_LengthRejected(t, expected, expected, len);
        }
    }
}

} // ayu::in
