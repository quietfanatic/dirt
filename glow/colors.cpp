#include "colors.h"

#include "../ayu/reflection/describe.h"
#include "../geo/vec.h"
#include "../uni/text.h"

using namespace glow;

static int from_hex_digit_for_color (char c) {
    int v = from_hex_digit(c);
    if (v < 0) raise(e_General, "Invalid color string for glow::RGBA8");
    return v;
}

AYU_DESCRIBE(glow::RGBA8,
    to_tree([](const RGBA8& v){
        UniqueString s (v.a != 255 ? 9 : 7);
        s[0] = '#';
        s[1] = to_hex_digit(v.r >> 4);
        s[2] = to_hex_digit(v.r & 0xf);
        s[3] = to_hex_digit(v.g >> 4);
        s[4] = to_hex_digit(v.g & 0xf);
        s[5] = to_hex_digit(v.b >> 4);
        s[6] = to_hex_digit(v.b & 0xf);
        if (v.a != 255) {
            s[7] = to_hex_digit(v.a >> 4);
            s[8] = to_hex_digit(v.a & 0xf);
        }
        return ayu::Tree(move(s));
    }),
    from_tree([](RGBA8& v, const ayu::Tree& t){
        switch (t.form) {
            case ayu::Form::Number: v = RGBA8(u32(t)); break;
            case ayu::Form::String: {
                auto s = Str(t);
                if (!s) raise(e_General, "Cannot use empty string for glow::RGBA8");
                if (s[0] != '#') raise(e_General, "Color string for glow::RGBA8 must start with #");
                if (s.size() == 4) {
                    int r = from_hex_digit_for_color(s[1]);
                    int g = from_hex_digit_for_color(s[2]);
                    int b = from_hex_digit_for_color(s[3]);
                    v.r = r << 4 | r;
                    v.g = g << 4 | g;
                    v.b = b << 4 | b;
                    v.a = 255;
                }
                else if (s.size() == 5) {
                    int r = from_hex_digit_for_color(s[1]);
                    int g = from_hex_digit_for_color(s[2]);
                    int b = from_hex_digit_for_color(s[3]);
                    int a = from_hex_digit_for_color(s[4]);
                    v.r = r << 4 | r;
                    v.g = g << 4 | g;
                    v.b = b << 4 | b;
                    v.a = a << 4 | a;
                }
                else if (s.size() == 7) {
                    int rh = from_hex_digit_for_color(s[1]);
                    int rl = from_hex_digit_for_color(s[2]);
                    int gh = from_hex_digit_for_color(s[3]);
                    int gl = from_hex_digit_for_color(s[4]);
                    int bh = from_hex_digit_for_color(s[5]);
                    int bl = from_hex_digit_for_color(s[6]);
                    v.r = rh << 4 | rl;
                    v.g = gh << 4 | gl;
                    v.b = bh << 4 | bl;
                    v.a = 255;
                }
                else if (s.size() == 9) {
                    int rh = from_hex_digit_for_color(s[1]);
                    int rl = from_hex_digit_for_color(s[2]);
                    int gh = from_hex_digit_for_color(s[3]);
                    int gl = from_hex_digit_for_color(s[4]);
                    int bh = from_hex_digit_for_color(s[5]);
                    int bl = from_hex_digit_for_color(s[6]);
                    int ah = from_hex_digit_for_color(s[7]);
                    int al = from_hex_digit_for_color(s[8]);
                    v.r = rh << 4 | rl;
                    v.g = gh << 4 | gl;
                    v.b = bh << 4 | bl;
                    v.a = ah << 4 | al;
                }
                else raise(e_General, "Invalid color string for glow::RGBA8");
                break;
            }
            case ayu::Form::Array: {
                auto a = Slice<ayu::Tree>(t);
                if (a.size() == 3) {
                    auto rgbf = geo::Vec3(
                        float(a[0]), float(a[1]), float(a[2])
                    );
                    for (u32 i = 0; i < 3; i++) {
                        if (rgbf[i] < 0 || rgbf[i] > 1) {
                            raise(e_General, "Component out of range for [r g b a] format; must be between 0 and 1.");
                        }
                    }
                    rgbf *= 255.f;
                    v = RGBA8(u8(rgbf[0]), u8(rgbf[1]), u8(rgbf[2]), 255);
                }
                else if (a.size() == 4) {
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
                }
                else {
                    ayu::raise_LengthRejected(
                        ayu::Type::For<RGBA8>(), 3, 4, a.size()
                    );
                }
                break;
            }
            default: ayu::raise_FromTreeFormRejected(
                ayu::Type::For<RGBA8>(), t.form
            );
        }
    })
)
