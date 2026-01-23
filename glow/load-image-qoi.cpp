#include "load-image.h"

#include "../uni/io.h"
#include "gl.h"

namespace glow {

namespace in {

[[noreturn, gnu::cold]] static
void raise_LoadImageFailed (Str filename, Str mess) {
     // TODO: tag
    raise(e_LoadImageFailed, cat(
        "Failed to load image from ", filename, ": ", mess
    ));
}

} using namespace in;

void load_texture_from_file (u32 target, AnyString filename) {
     // TODO: detect 3-channel file and use GL_RGB8
    UniqueImage image = load_image_from_file(move(filename));
     // Now upload texture
    require(image.size.x * image.size.y > 0);
    glTexImage2D(
        target, 0, GL_RGBA8,
        image.size.x, image.size.y, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, image.pixels
    );
}

static u32 read_u32 (const u8* in) {
    return in[0] << 24 | in[1] << 16 | in[2] << 8 | in[3] << 0;
}

static constexpr
u8 hash_pixel (u8 r, u8 g, u8 b, u8 a) {
    return (r*3 + g*5 + b*7 + a*11) % 64;
}

 // Returns 0 if successful, + if too much input, - if too little input.
NOINLINE static
int decode_qoi (RGBA8* out, RGBA8* out_end, const u8* in, const u8* in_end) {
    RGBA8 history [64] = {};
     // We're primarily keeping the pixel coalesced in one register, because the
     // two most common ops (index and run) only care about the coalesced form.
     // However, predeclare the individual registers so we can use them across
     // the goto.
    RGBA8 px = {0, 0, 0, 255};
     // These have to be u8 because they are specced to wrap around.
    u8 r, g, b, a;
     // Special semi- or fully-paranoid tweak to allow an optimization. [1]
#ifdef GLOW_PARANOID_QOI_DECODE
    bool start_run = *in >= 0b11000000 && *in < 0b11111110;
    history[59].a = -start_run;
#else
    history[59].a = 255;
#endif
    while (out < out_end && in < in_end) {
         // Ordering these by rough likelihood for a pixel-art game with
         // transparent sprites.
        if (*in < 0b01000000) [[likely]] { // QOI_OP_INDEX
            u8 index = *in & 0b00111111;
            px.repr = history[index].repr;
            (out++)->repr = px.repr;
#ifdef GLOW_PARANOID_QOI_DECODE
            if (px.repr == 0) [[unlikely]] history[0].repr = px.repr;
#endif
            in += 1;
            continue;
        }
        else if (*in >= 0b11000000) {
            if (*in < 0b11111110) [[likely]] { // QOI_OP_RUN
                u32 len = (*in & 0b00111111) + 1;
                if (len > out_end - out) [[unlikely]] break;
                 // This seems to convince the compiler to vectorize this loop
                 // in a non-awful way.
                do { (out++)->repr = px.repr; } while (--len);
                in += 1;
                continue;
            }
            else { // QOI_OP_RGB or QOI_OP_RGBA
                r = in[1];
                g = in[2];
                b = in[3];
                if (*in == 0b11111111) {
                    a = in[4];
                    in += 5;
                }
                else {
                    a = px.a;
                    in += 4;
                }
                goto new_pixel;
            }
        }
        else if (*in >= 0b10000000) [[likely]] { // QOI_OP_LUMA
            i8 dg = (*in & 0b00111111) - 32;
            i8 du = ((in[1] & 0b11110000) >> 4) - 8;
            i8 dv = ((in[1] & 0b00001111) >> 0) - 8;
            r = px.r + du + dg;
            g = px.g + dg;
            b = px.b + dv + dg;
            a = px.a;
            in += 2;
            goto new_pixel;
        }
        else { // QOI_OP_DIFF
            r = px.r + ((*in & 0b00110000) >> 4) - 2;
            g = px.g + ((*in & 0b00001100) >> 2) - 2;
            b = px.b + ((*in & 0b00000011) >> 0) - 2;
            a = px.a;
            in += 1;
            goto new_pixel;
        }
        new_pixel:
        px = {r, g, b, a};
        (out++)->repr = px.repr;
        history[hash_pixel(r, g, b, a)].repr = px.repr;
    }
    return in_end - in;
}

UniqueImage load_image_from_file (AnyString filename) {
    UniqueString file = string_from_file(filename);
    if (file.size() < 14 + 8) raise_LoadImageFailed(filename, "File is too short");
    if (file.slice(0, 4) != "qoif") raise_LoadImageFailed(filename, "File is not QOI format");
    if (file.slice(file.size()-8) != "\x00\x00\x00\x00\x00\x00\x00\x01") {
        raise_LoadImageFailed(filename, "QOI file doesn't end properly");
    }
    const u8* in = file.reinterpret<u8>().begin();
    const u8* in_end = file.reinterpret<u8>().end();
    u32 width = read_u32(in + 4);
    u32 height = read_u32(in + 8);
    u64 len = width * height;
    if (len > 400000000) raise_LoadImageFailed(filename, "Image is too large");
     // Ignore channels and colorspace for now.

    UniqueImage r (IVec(width, height));
    RGBA8* out = r.pixels;
    RGBA8* out_end = out + len;
    in += 14;
    in_end -= 8;

    int res = decode_qoi(out, out_end, in, in_end);
    if (res) {
        raise_LoadImageFailed(filename,
            res > 0 ? Str("Too much data") : Str("Not enough data")
        );
    }

    return r;
}

} using namespace glow;

///// FOOTNOTES
 // [1]  So.  According to the spec and the reference implementation, every
 // pixel is supposed to update the history array.  We're skipping that step for
 // QOI_OP_RUN and QOI_OP_INDEX, because they use pixels that have already been
 // entered into the history.  This optimization is valid in almost all cases.
 // HOWEVER!  It breaks down if the input starts with QOI_OP_RUN.  This is
 // because the initial values for the history and the last-seen-pixel are
 // inconsistent: the history is filled with 0s, but the lsp has a=255, so
 // officially, a starting run ought to set history[59] to {0,0,0,255}.  Now,
 // when I export a QOI with The GIMP, it doesn't seem to do this (though the
 // file is still conforming, because using history is optional).  The reference
 // encoder also does not update history on a run.  But a different encoder
 // could do this, so we must assume it could happen.
 //
 // We are still, however, going to cheat a little, in that we won't check for
 // an initial run, we'll just set the history entry always.  In theory, a
 // conforming encoder COULD emit a QOI_OP_INDEX that uses this history entry,
 // assuming it has been properly initialized to {0,0,0,0}.  That would
 // be weird, and borderline malicious, when the proper entry for that color is
 // entry 0.  I suppose I could see a hyper-aggressively compressing encoder
 // doing so, if it's filled the proper history entry with something else, so it
 // gets its {0,0,0,0} from an improper entry.  And if it does that, then
 // according to the spec even QOI_OP_INDEX must update the history, meaning
 // that indexing an improper entry for {0,0,0,0} would set the proper entry to
 // {0,0,0,0}, which would threaten our optimization even more.
 //
 // So I'm just gonna assume that will never happen.  If an encoder wants to
 // spend that much effort to save a pittance of bytes--at most 252 for an
 // specially-crafted file--it should be spending those cycles on DEFLATE or
 // something instead.  You can #define GLOW_PARANOID_QOI_DECODE to make sure
 // this scenario is covered.
