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

static u8 hash_pixel (u8 r, u8 g, u8 b, u8 a) {
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
    while (out < out_end && in < in_end) {
         // Ordering these by rough likelihood for a pixel-art game with
         // transparent sprites.
        if (*in < 0b01000000) [[likely]] { // QOI_OP_INDEX
            u8 index = *in & 0b00111111;
            px = history[index];
            *out++ = px;
            in += 1;
            continue;
        }
        else if (*in >= 0b11000000) {
            if (*in < 0b11111110) { // QOI_OP_RUN
                u32 len = (*in & 0b00111111) + 1;
                if (len > out_end - out) break;
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
                a = px.a;
                if (*in == 0b11111111) {
                    a = in[4];
                    in += 1;
                }
                in += 4;
                goto new_pixel;
            }
        }
        else if (*in >= 0b10000000) { // QOI_OP_LUMA
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
        *out++ = px;
        history[hash_pixel(r, g, b, a)] = px;
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

