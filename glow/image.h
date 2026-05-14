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
     // Distance in pixels between rows.  If the image is stored contiguously,
     // this will be size.x.  This can be negative, in which case the image is
     // flipped vertically.
    i32 stride;
     // Pointer to first pixel.
    RGBA8* pixels = null;
    constexpr ImageView () { }
    constexpr ImageView (IVec s, RGBA8* p) : size(s), stride(s.x), pixels(p) {
        expect(s.x >= 0 && s.y >= 0);
    }
    constexpr ImageView (IVec s, i32 t, RGBA8* p) : size(s), stride(t), pixels(p) {
        expect(s.x >= 0 && s.y >= 0);
    }

     // The bounds of the image as a rectangle.  Note that for y-down style
     // images, this will be upside-down; bounds().b will be on the top.
    constexpr IRect bounds () const { return {{0, 0}, size}; }

    constexpr const RGBA8& operator [] (IVec i) const {
        expect(pixels);
        expect(contains(bounds(), i));
        return pixels[stride * i.y + i.x];
    }

     // Get a new view constrained to a smaller rectangle.
    constexpr ImageView crop (const IRect& b) const {
        expect(contains(bounds(), b));
        return ImageView(
            geo::size(b), stride, pixels + (stride * b.b + b.l)
        );
    }
     // Flip view vertically without reallocating, swapping the top and bottom.
    constexpr ImageView flipy () const {
        return ImageView(size, -stride, pixels + stride * (size.y-1));
    }
     // If true, all pixels are contiguous in memory and can be copied with a
     // single call to memcpy().  This is the case if the ImageView was
     // converted directly from a UniqueImage.
    constexpr bool contiguous () const {
        return stride == size.x;
    }
};

 // Copy pixels from one image region to another.  They must have exactly the
 // same size vector, but can be empty.
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

    ImageView crop (const IRect& b) { return ImageView(*this).crop(b); }
    ImageView flipy () { return ImageView(*this).flipy(); }
};

 // Load from memory.  NYI for non-QOI formats.
UniqueImage image_from_blob (Slice<u8> content);
UniqueImage image_from_blob_qoi (Slice<u8> blob);
 // Load from a file.  Will use SAIL if GLOW_USE_SAIL is defined, otherwise can
 // only load QOI files.
UniqueImage image_from_file (const char* filepath);
UniqueImage image_from_file (Str filepath);
UniqueImage image_from_file_qoi (const char* filepath);
UniqueImage image_from_file_qoi (Str filepath);
#ifdef GLOW_USE_SAIL
UniqueImage image_from_file_sail (const char* filepath);
UniqueImage image_from_file_sail (Str filepath);
#endif

constexpr ErrorCode e_LoadImageFailed = "glow::e_LoadImageFailed";

 // Write to memory.  NYI for non-qoi formats.
UniqueArray<u8> image_to_blob (const ImageView&);
UniqueArray<u8> image_to_blob_qoi (const ImageView&);
 // Write to file.  NYI for non-qoi formats.
void image_to_file (const ImageView&, const char* filepath);
void image_to_file (const ImageView&, Str filepath);
void image_to_file_qoi (const ImageView&, const char* filepath);
void image_to_file_qoi (const ImageView&, Str filepath);

constexpr ErrorCode e_SaveImageFailed = "glow::e_SaveImageFailed";

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
