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
        if (t.form == ayu::NUMBER) {
            v = RGBA8(int32(t));
        }
        else if (t.form == ayu::ARRAY) {
            auto a = ayu::TreeArraySlice(t);
            if (a.size() != 4) {
                throw ayu::WrongLength(
                    ayu::current_location(),
                    ayu::Type::CppType<RGBA8>(),
                    4, 4, a.size()
                );
            }
            v = RGBA8(uint8(a[0]), uint8(a[1]), uint8(a[2]), uint8(a[3]));
        }
        else throw ayu::InvalidForm(
            ayu::current_location(),
            ayu::Type::CppType<RGBA8>(),
            t
        );
    })
)
