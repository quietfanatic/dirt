#pragma once

#include <optional>
#include "../ayu/resources/extension.h"
#include "../geo/rect.h"
#include "colors.h"

namespace glow {

 // Non-owning view of an image.
struct ImageView {
     // The width and height in pixels.  This is a signed IVec but neither
     // component can be negative.  Because of this, images cannot be more than
     // 2 billion x 2 billion pixels.  I hope you'll forgive me.
    IVec size;
     // Distance between rows in pixels.  If the row are stored contiguously,
     // this is equal to size.x.
    i32 stride = 0;
     // Pointer to pixel data, arranged top-down left-to-right.
     //   {0, 0}, {1, 0}, {0, 1}, {1, 1}
    RGBA8* pixels = null;
    constexpr ImageView () { }
    constexpr ImageView (IVec s, RGBA8* p) : size(s), stride(s.x), pixels(p) { }
    constexpr ImageView (IVec s, i32 t, RGBA8* p) : size(s), stride(t), pixels(p) { }

     // The bounds of the image as a rectangle.  Note that this will be
     // upside-down; bounds().b refers to the top of the image.
    constexpr IRect bounds () const { return {{0, 0}, size}; }

    constexpr const RGBA8& operator [] (IVec i) const {
        expect(pixels);
        expect(contains(bounds(), i));
        return pixels[i.y * stride + i.x];
    }

    constexpr ImageView subview (const IRect& b) {
        expect(contains(bounds(), b));
        return ImageView(geo::size(b), stride, &pixels[b.b * stride + b.l]);
    }
};

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

constexpr ErrorCode e_LoadImageFailed = "glow::e_LoadImageFailed";

} // glow

 // [1]
 // We're directly calling malloc and free instead of using lilac, because most
 // images are too large for lilac's small allocator (they'd have to be smaller
 // than 32x32), and because SAIL uses malloc/free internally, so this allows us
 // to steal its buffers.
