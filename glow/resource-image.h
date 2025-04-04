#pragma once
#include "../iri/iri.h"
#include "common.h"
#include "image.h"

namespace glow {

 // An image that can lazily load itself from a file.
struct ResourceImage : Image {
    IRI source;
    UniqueImage storage;

    constexpr ResourceImage () { }
    ResourceImage (const IRI& s) : source(s) { }
    ~ResourceImage () { }

    void load ();
    void trim ();
    operator ImageRef ();

    ImageRef Image_data () override { return ImageRef(*this); }
    void Image_trim () override { trim(); }
};

} // glow
