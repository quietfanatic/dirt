#pragma once
#include "../ayu/resources/extension.h"
#include "../iri/iri.h"
#include "common.h"
#include "image.h"

namespace glow {

 // An image that can lazily load itself from a file.  Intended to be an AYU
 // resource type.
struct FileImage : Image {
    const SharedString source;
    UniqueImage storage;

    constexpr FileImage () { }
     // Don't load
    FileImage (const SharedString& s) : source(s) { }
     // Do load
    FileImage (const SharedString& s, Slice<u8> blob);

    ~FileImage () { }

    void load ();
     // Free pixels.  Will be reloaded when requested.
    void trim ();
     // Autoload.
    operator ImageRef ();

    ImageRef Image_data () override { return ImageRef(*this); }
    void Image_trim () override { trim(); }
};

struct FileImageExtension : ayu::ResourceExtension {
    bool accepts_type (ayu::Type) override; // Only accepts FileImage
    void from_blob (ayu::AnyVal&, Slice<u8>, ayu::ResourceRef, ayu::ResourceScheme*) override;
    UniqueArray<u8> to_blob (const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions) override;
    using ayu::ResourceExtension::ResourceExtension;
};

} // glow
