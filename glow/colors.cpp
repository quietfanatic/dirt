#include "colors.h"

#include "../ayu/describe.h"

using namespace glow;

AYU_DESCRIBE(glow::RGBA8,
    to_tree([](const RGBA8& v){
        auto r = ayu::Tree(uint32(v));
        r.flags |= ayu::PREFER_HEX;
        return r;
    }),
    from_tree([](RGBA8& v, const ayu::Tree& t){
        switch (t.form) {
            case ayu::Form::Number: v = RGBA8(int32(t)); break;
            case ayu::Form::Array: {
                auto a = ayu::TreeArraySlice(t);
                if (a.size() != 4) {
                    ayu::raise_LengthRejected(
                        ayu::Type::CppType<RGBA8>(), 4, 4, a.size()
                    );
                }
                v = RGBA8(uint8(a[0]), uint8(a[1]), uint8(a[2]), uint8(a[3]));
                break;
            }
            default: ayu::raise_FromTreeFormRejected(
                ayu::Type::CppType<RGBA8>(), t.form
            );
        }
    })
)
