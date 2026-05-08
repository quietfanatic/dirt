#pragma once

#include <optional>
#include "../ayu/resources/extension.h"
#include "../geo/rect.h"
#include "../geo/vec.h"
#include "colors.h"

namespace glow {
using namespace geo;

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
};

 // A generic interface for fetching image data
struct ImageSource {
    using GetterFunc = ImageView(ImageSource&);
    GetterFunc& getter;
    ImageView get () { return getter(*this); }
};

 // Mixin to implement ImageSource based on conversion to ImageView
template <class T>
struct ImageSourceImp : ImageSource {
    static ImageView get_ (ImageSource& s0) {
        auto& s1 = static_cast<ImageSourceImp<T>&>(s0);
        auto& s2 = static_cast<T&>(s1);
        return ImageView(s2);
    }
    constexpr ImageSourceImp () : ImageSource(get_) { }
};

 // An image that owns its pixels and cannot be trimmed.
struct UniqueImage : ImageSourceImp<UniqueImage> {
    IVec size;
     // The pixel buffer is allocated with std::malloc.  If you steal it you
     // need to deallocate it with std::free.
    RGBA8* pixels;

    constexpr UniqueImage () : pixels(null) { }
     // Create from already-allocated pixels.
    constexpr UniqueImage (IVec s, RGBA8*&& p) :
        size(s), pixels(p)
    { p = nullptr; }
     // Allocate new pixels array.  The contents are undefined.
    explicit UniqueImage (IVec size) noexcept;

    constexpr UniqueImage (UniqueImage&& o) :
        UniqueImage(o.size, move(o.pixels))
    { }
    UniqueImage& operator= (UniqueImage&& o) {
        this->~UniqueImage();
        new (this) UniqueImage(move(o));
        return *this;
    }

    ~UniqueImage ();

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

 // An image that can lazily load itself from a file.  Intended to be an AYU
 // resource type.
struct FileImage : ImageSourceImp<FileImage> {
     // Serialized
    SharedString filepath;
     // Not serialized
    IVec size;
    RGBA8* pixels;

     // Don't load
    constexpr FileImage (const SharedString& p = "") : filepath(p) { }
     // Do load // TODO: deprecate in favor of setting members
    FileImage (const SharedString& p, Slice<u8> encoded);

    ~FileImage () { }

     // Autoload
    operator ImageView ();
     // Free memory until next autoload.  Size remains.
    void trim ();
};

 // Allow using image files as AYU resources
struct FileImageExtension : ayu::ResourceExtension {
    bool accepts_type (ayu::Type) override; // Only accepts FileImage
    void from_blob (ayu::AnyVal&, Slice<u8>, ayu::ResourceRef, ayu::ResourceScheme*) override;
    UniqueArray<u8> to_blob (const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions) override;
    using ayu::ResourceExtension::ResourceExtension;
};

constexpr ErrorCode e_SubImageBoundsNotProper = "glow::SubImageBoundsNotProper";
constexpr ErrorCode e_SubImageOutOfBounds = "glow::SubImageOutOfBounds";
constexpr ErrorCode e_LoadImageFailed = "glow::e_LoadImageFailed";

 // Don't use lilac for these allocations, because they're almost guaranteed to
 // be so large they get passed on to malloc anyway (they'd have to be smaller
 // than 24x24 to use the small-size allocator).  Also, libsail uses
 // malloc/free internally, so matching that allows us to steal its buffers.
inline UniqueImage::UniqueImage (IVec s) noexcept :
    size((require(area(s) >= 0), s)),
    pixels((RGBA8*)std::malloc(area(size) * sizeof(RGBA8)))
{ }

inline UniqueImage::~UniqueImage () { std::free(pixels); }

inline FileImage::FileImage (const SharedString& p, Slice<u8> encoded) :
    filepath(p)
{
    auto img = image_from_blob(encoded);
    size = img.size;
    pixels = img.pixels;
    img.pixels = null;
}

inline FileImage::operator ImageView () {
    if (!pixels) {
        auto img = image_from_file(filepath);
        size = img.size;
        pixels = img.pixels;
        img.pixels = null;
    }
    return ImageView(size, pixels);
}
inline void FileImage::trim () {
    if (pixels) { // Keep size
        std::free(pixels);
        pixels = null;
    }
}

} // glow
