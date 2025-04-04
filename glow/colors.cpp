#include "colors.h"

#include "../geo/vec.h"
#include "../ayu/reflection/describe.h"

using namespace glow;

AYU_DESCRIBE(glow::RGBA8,
    to_tree([](const RGBA8& v){
        return ayu::Tree(u32(v), ayu::TreeFlags::PreferHex);
    }),
    from_tree([](RGBA8& v, const ayu::Tree& t){
        switch (t.form) {
             // TODO: convert from #rrggbbaa string
            case ayu::Form::Number: v = RGBA8(u32(t)); break;
            case ayu::Form::Array: {
                auto a = Slice<ayu::Tree>(t);
                if (a.size() != 4) {
                    ayu::raise_LengthRejected(
                        ayu::Type::CppType<RGBA8>(), 4, 4, a.size()
                    );
                }
                auto rgbaf = geo::Vec4(
                    float(a[0]), float(a[1]), float(a[2]), float(a[3])
                );
                for (u32 i = 0; i < 4; i++) {
                    if (rgbaf[i] < 0 || rgbaf[i] > 1) {
                        raise(e_General, "Component out of range for [r g b a] format; must be between 0 and 1.");
                    }
                }
                rgbaf *= 255.f;
                v = RGBA8(u8(rgbaf[0]), u8(rgbaf[1]), u8(rgbaf[2]), u8(rgbaf[3]));
                break;
            }
            default: ayu::raise_FromTreeFormRejected(
                ayu::Type::CppType<RGBA8>(), t.form
            );
        }
    })
)
