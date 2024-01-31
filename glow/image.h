#pragma once

#include <optional>
#include "../geo/rect.h"
#include "../geo/vec.h"
#include "colors.h"

namespace glow {
using namespace geo;

struct ImageRef {
     // The width and height in pixels.
    IVec size;
     // Distance between rows in pixels.  If all the pixels are stored
     // contiguously, this should be equal to size.x
    usize stride = 0;
     // Pointer to pixel data, arranged top-down left-to-right.
     //   {0, 0}, {1, 0}, {0, 1}, {1, 1}
    RGBA8* pixels = null;
    constexpr ImageRef () { }
    constexpr ImageRef (IVec s, RGBA8* p) : size(s), stride(s.x), pixels(p) { }
    constexpr ImageRef (IVec s, usize t, RGBA8* p) : size(s), stride(t), pixels(p) { }

     // The bounds of the image as a rectangle.  Note that this will be
     // upside-down; bounds().b refers to the top of the image.
    constexpr IRect bounds () const { return {{0, 0}, size}; }

    constexpr const RGBA8& operator [] (IVec i) const {
        expect(pixels);
        expect(contains(bounds(), i));
        return pixels[i.y * stride + i.x];
    }
};

 // A generic interface for images that can be lazily loaded.
struct Image {
     // Load and return image data.
    virtual ImageRef Image_data () = 0;
     // Clear lazily-loaded data
    virtual void Image_trim () { };
};

 // An image that owns its pixels and cannot be trimmed.
struct UniqueImage : Image {
    IVec size;
     // The pixel buffer is allocated with std::malloc.  If you steal it you
     // need to deallocate it with std::free.
    RGBA8* pixels;

    constexpr UniqueImage () : pixels(null) { }
     // Create from already-allocated pixels.
    UniqueImage (IVec s, RGBA8*&& p) : size(s), pixels(p) { p = nullptr; }
     // Allocate new pixels array.  The contents are undefined.
    explicit UniqueImage (IVec size) noexcept;

    constexpr UniqueImage (UniqueImage&& o) : size(o.size), pixels(o.pixels) {
        o.pixels = null;
    }
    UniqueImage& operator= (UniqueImage&& o) {
        this->~UniqueImage();
        size = o.size; pixels = o.pixels; o.pixels = null;
        return *this;
    }

    ~UniqueImage ();

    constexpr explicit operator bool () const { return pixels; }
    IRect bounds () const { return {{0, 0}, size}; }
    constexpr operator ImageRef () const { return {size, pixels}; }

    RGBA8& operator [] (IVec i) {
        expect(pixels);
        expect(contains(bounds(), i));
        return pixels[i.y * size.x + i.x];
    }
    const RGBA8& operator [] (IVec i) const {
        expect(pixels);
        expect(contains(bounds(), i));
        return pixels[i.y * size.x + i.x];
    }

    ImageRef Image_data () override { return {size, pixels}; }
};

 // Const reference type that refers to a portion of another image.
struct SubImage {
     // Image that is being referenced.
    Image* image = null;
     // Area of the subimage in pixels.  Coordinates refer to the corners
     // between pixels, not the pixels themselves.  As a special case, GINF
     // refers to the entire image.  Otherwise, cannot have negative width or
     // height and cannot be outside the bounds of the image.
    IRect bounds = GINF;

     // Will throw if bounds is outside the image or is not proper.
     // Can't check if the bounds or image size is changed later.
    void validate ();

    constexpr SubImage () { }
    SubImage (Image* image, const IRect& bounds = GINF) :
        image(image), bounds(bounds)
    { validate(); }

    constexpr explicit operator bool () { return image; }

    operator ImageRef () const {
        auto data = image->Image_data();
        if (bounds != GINF) {
            require(contains(data.bounds(), bounds));
            return ImageRef(
                geo::size(bounds),
                data.stride,
                data.pixels + bounds.b * data.stride + bounds.l
            );
        }
        else return data;
    }
};

constexpr ayu::ErrorCode e_SubImageBoundsNotProper = "glow::SubImageBoundsNotProper";
constexpr ayu::ErrorCode e_SubImageOutOfBounds = "glow::SubImageOutOfBounds";

}
