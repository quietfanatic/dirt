#include "image-transform.h"

#include <unordered_map>
#include "../ayu/reflection/describe.h"

namespace glow {

UniqueImage ReplaceColor::apply (const ImageRef& in, BVec flip) {
    IRect bounds = in.bounds();
    UniqueImage out (in.size);
    for (i32 y = 0; y < out.size.y; y++)
    for (i32 x = 0; x < out.size.x; x++) {
        RGBA8 pixel = in[{
            flip.x ? bounds.r - x - 1 : bounds.l + x,
            flip.y ? bounds.t - y - 1 : bounds.b + y
        }];
        if (pixel == from) pixel = to;
        out[{x, y}] = pixel;
    }
    return out;
}

UniqueImage copy_pixels (const ImageRef& in, BVec flip) {
    IRect bounds = in.bounds();
    UniqueImage out (in.size);
    for (i32 y = 0; y < out.size.y; y++)
    for (i32 x = 0; x < out.size.x; x++) {
        out[{x, y}] = in[{
            flip.x ? bounds.r - x - 1 : bounds.l + x,
            flip.y ? bounds.t - y - 1 : bounds.b + y
        }];
    }
    return out;
}

//UniqueImage Palette::apply (const ImageRef& in, BVec flip) {
//    IRect bounds = in.bounds();
//
//    IRect minimum_area = horizontal
//        ? IRect{pos + IVec{-1, 0}, pos + IVec{0, 1}}
//        : IRect{pos + IVec{0, -1}, pos + IVec{1, 0}};
//    if (!contains(bounds, minimum_area)) {
//        raise(e_General,
//            "Palette pos is at edge of or outside of image."
//        );
//    }
//
//     // Generate map
//    ImageRef data = image->Image_data();
//    std::unordered_map<RGBA8, RGBA8> map;
//    if (horizontal) {
//        RGBA8* background = data[{pos + IVec{-1, 0}}];
//        if (data[{pos + IVec{-1, 1}] != background) {
//            raise(e_General,
//                "Couldn't determine Palette size because the pixels to "
//                "its left are different."
//            );
//        }
//        for (i32 x = pos.x; x < data.size.x; ++x) {
//            RGBA8 from = data[{x, pos.y}];
//            RGBA8 to = data[{x, pos.y + 1}];
//            if (from == background && to == background) break;
//            map.emplace(from, to);
//        }
//    }
//    else {
//        RGBA8* background = data[{pos + IVec{0, -1}}];
//        if (data[{pos + IVec{1, -1}] != background) {
//            raise(e_General,
//                "Couldn't determine Palette size because the pixels "
//                "above it are different."
//            );
//        }
//        for (i32 y = pos.y; x < data.size.y; ++y) {
//            RGBA8 from = data[{pos.x, y}];
//            RGBA8 to = data[{pos.x + 1, y}];
//            if (from == background && to == background) break;
//            map.emplace(from, to);
//        }
//    }
//
//     // Copy pixels while applying map
//    UniqueImage out (in.size);
//    for (i32 y = 0; y < out.size.y; y++)
//    for (i32 x = 0; x < out.size.x; x++) {
//        RGBA8 from = in[{
//            flip.x ? bounds.r - x - 1 : bounds.l + x,
//            flip.y ? bounds.t - y - 1 : bounds.b + y
//        }];
//        auto it = map.find(from);
//        RGBA8 to = it != map.end() ? map->second : from;
//        out[{x, y}] = to;
//    }
//    return out;
//}

} using namespace glow;

AYU_DESCRIBE(glow::ReplaceColor,
    elems(
        elem(&ReplaceColor::from),
        elem(&ReplaceColor::to)
    )
)
