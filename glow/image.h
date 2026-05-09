#pragma once

#include <optional>
#include "../ayu/resources/extension.h"
#include "../geo/rect.h"
#include "colors.h"

namespace glow {

 // Non-owning view of an image.  Some basic transformations can be done without
 // copying any data (flips, 90-degree rotations, cropping to rectangle).
struct ImageView {
     // The width and height in pixels.  This is a signed IVec but neither
     // component can be negative.  Because of this, images cannot be more than
     // 2 billion x 2 billion pixels.  I hope you'll forgive me.
    IVec size;
     // Distance in RGBA8* pointer units between pixels in each dimension.  If
     // the image is stored contiguously, this will be [1 size.x].
    IVec stride;
     // Pointer to first pixel.
    RGBA8* pixels = null;
    constexpr ImageView () { }
    constexpr ImageView (IVec s, RGBA8* p) : size(s), stride(1, s.x), pixels(p) { }
    constexpr ImageView (IVec s, IVec t, RGBA8* p) : size(s), stride(t), pixels(p) { }

     // The bounds of the image as a rectangle.  Note that this will be
     // upside-down; bounds().b refers to the top of the image.
    constexpr IRect bounds () const { return {{0, 0}, size}; }

    constexpr const RGBA8& operator [] (IVec i) const {
        expect(pixels);
        expect(contains(bounds(), i));
        return pixels[dot(stride, i)];
    }

    constexpr ImageView crop (const IRect& b) const {
        expect(contains(bounds(), b));
        return ImageView(
            geo::size(b), stride, pixels + dot(stride, lb(b))
        );
    }
    constexpr ImageView flipx () const {
        ImageView r = *this;
        r.pixels += stride.x * (size.x - 1);
        r.stride.x = -stride.x;
        return r;
    }
    constexpr ImageView flipy () const {
        ImageView r = *this;
        r.pixels += stride.y * (size.y - 1);
        r.stride.y = -stride.y;
        return r;
    }
     // Flips along the diagonal, keeping the [0 0] corner constant.
    constexpr ImageView flipxy () const {
        return ImageView({size.y, size.x}, {stride.y, stride.x}, pixels);
    }
    constexpr ImageView rotcw () const {
        return flipy().flipxy();
    }
    constexpr ImageView rotccw () const {
        return flipx().flipxy();
    }
    constexpr ImageView rot180 () const {
        return flipx().flipy();
    }

     // Flip according to a BVec
    constexpr ImageView flip (BVec f) const {
        ImageView r = *this;
        if (f.x) r = r.flipx();
        if (f.y) r = r.flipy();
        return r;
    }

     // If true, all pixels are contiguous in memory and can be copied with a
     // single call to memcpy().  This is the case if the ImageView was
     // converted directly from a UniqueImage.
    constexpr bool contiguous () const {
        return (stride.x == 1) & (stride.y == size.x);
    }
//     // Transform based on a highly restricted matrix.  The matrix must be
//     // diagonal or antidiagonal, and must have two 0s and two +/- 1s.
//    constexpr ImageView transform (const GMat<i32, 2, 2>& mat) const {
//        GMat<bool, 2, 2> abs = {
//            !!mat[0][0], !!mat[0][1], !!mat[1][0], !!mat[1][1]
//        };
//        GMat<bool, 2, 2> sign = {
//            mat[0][0] < 0, mat[0][1] < 0, mat[1][0] < 0, mat[1][1] < 0
//        };
//         // Make it so 1 maps to 0 and -1 maps to size - 1
//        IVec adjuster = mat * 
//        return ImageView(
//            abs * size,
//            mat * stride,
//            pixels + sign * (size - 1)
//        );
//    }
};

void blit (const ImageView& out, const ImageView& b) noexcept;

 // An image that owns its pixels and cannot be trimmed.
struct UniqueImage {
    IVec size;
     // The pixel buffer is allocated with std::malloc.  If you steal it you
     // need to deallocate it with std::free. [1]
    RGBA8* pixels;

    constexpr UniqueImage () : pixels(null) { }

     // Create from already-allocated pixels.
    constexpr UniqueImage (IVec s, RGBA8*&& p) :
        size(s), pixels(p)
    { p = nullptr; }

     // Allocate new pixels array.  The contents are undefined.
    explicit UniqueImage (IVec s) noexcept :
        size((require(area(s) >= 0), s)),
        pixels((RGBA8*)std::malloc(area(s) * sizeof(RGBA8)))
    { }

    constexpr UniqueImage (UniqueImage&& o) :
        UniqueImage(o.size, move(o.pixels))
    { }
    UniqueImage& operator= (UniqueImage&& o) {
        this->~UniqueImage();
        new (this) UniqueImage(move(o));
        return *this;
    }

    ~UniqueImage () { std::free(pixels); }

    operator ImageView () { return {size, pixels}; }

    constexpr explicit operator bool () const { return pixels; }

    IRect bounds () const { return {{0, 0}, size}; }

    RGBA8& operator [] (IVec i) {
        expect(pixels);
        expect(contains(bounds(), i));
        return pixels[i.y * size.x + i.x];
    }
};

 // Load from memory.  NYI for non-QOI formats.
UniqueImage image_from_blob (Slice<u8> content, Str filepath = "");
UniqueImage image_from_blob_qoi (Slice<u8> blob, Str filepath = "");
 // Load from a file.  Will use SAIL if GLOW_USE_SAIL is defined, otherwise can
 // only load QOI files.
UniqueImage image_from_file (SharedString filepath);
UniqueImage image_from_file_qoi (SharedString filepath);
#ifdef GLOW_USE_SAIL
UniqueImage image_from_file_sail (SharedString filepath);
#endif

constexpr ErrorCode e_LoadImageFailed = "glow::e_LoadImageFailed";

 // An image that is (potentially) associated with a file so it can be trimmed
 // and reloaded.  Intended to be an AYU resource type.
struct FileImage : UniqueImage {
    SharedString filepath;

     // Set filepath but don't load
    constexpr FileImage (const SharedString& p = "") : filepath(p) { }

     // Construct already loaded
    constexpr FileImage (const SharedString& p, UniqueImage&& img) :
        UniqueImage(move(img)), filepath(p)
    { }

    ~FileImage () { }

     // Load if unloaded
    void load () {
        if (!pixels) {
            static_cast<UniqueImage&>(*this) = image_from_file(filepath);
        }
    }
     // Free memory until next load.  Size remains.
    void trim () {
        if (!filepath) return; // Can't trim if no backing file!
        std::free(pixels); // no-op if null
        pixels = null; // Keep size
    }
};

 // Allow using image files as AYU resources
struct FileImageExtension : ayu::ResourceExtension {
    bool accepts_type (ayu::Type) override; // Only accepts FileImage
    void from_blob (ayu::AnyVal&, Slice<u8>, ayu::ResourceRef, ayu::ResourceScheme*) override;
    UniqueArray<u8> to_blob (const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions) override;
    using ayu::ResourceExtension::ResourceExtension;
};

} // glow

 // [1]
 // We're directly calling malloc and free instead of using lilac, because most
 // images are too large for lilac's small allocator (they'd have to be smaller
 // than 32x32), and because SAIL uses malloc/free internally, so this allows us
 // to steal its buffers.
