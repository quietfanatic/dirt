#include "colors.h"

#include "../ayu/reflection/describe.h"

using namespace glow;

AYU_DESCRIBE(glow::RGBA8,
    to_tree([](const RGBA8& v){
        return ayu::Tree(uint32(v), ayu::TreeFlags::PreferHex);
    }),
    from_tree([](RGBA8& v, const ayu::Tree& t){
        switch (t.form) {
            case ayu::Form::Number: v = RGBA8(uint32(t)); break;
            case ayu::Form::Array: {
                auto a = Slice<ayu::Tree>(t);
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
