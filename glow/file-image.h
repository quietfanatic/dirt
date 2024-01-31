#pragma once
#include "../iri/iri.h"
#include "common.h"
#include "image.h"

struct SDL_Surface;

namespace glow {

 // An image that can lazily load itself from a file.
struct FileImage : Image {
    IRI source;
     // I can't figure out how to steal the pixels from an SDL surface, so we'll
     // just have to use it as is.
    SDL_Surface* storage = null;
    constexpr FileImage () { }
    FileImage (const IRI& s) : source(s) { }
    ~FileImage ();

    void load ();
    void trim ();
    operator ImageRef ();

    ImageRef Image_data () override { return ImageRef(*this); }
    void Image_trim () override { trim(); }
};

constexpr uni::ErrorCode e_FileImageLoadFailed = "glow::e_FileImageLoadFailed";

} // glow
