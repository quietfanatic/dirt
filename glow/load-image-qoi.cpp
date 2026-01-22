#include "load-image.h"

#include "../uni/io.h"
#include "gl.h"

namespace glow {

namespace in {
[[noreturn, gnu::cold]] static
void raise_LoadImageFailed (Str, Str);
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

static u8 hash_pixel (RGBA8 px) {
    return (px.r*3 + px.g*5 + px.b*7 + px.a*11) % 64;
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

    RGBA8 history [64] = {};
    RGBA8 current = {0,0,0,255};
    while (out != out_end) {
        if (in == in_end) raise_LoadImageFailed(filename, "Not enough data");
        switch (*in & 0b11000000) {
            case 0b00000000: {
                u8 index = *in & 0b00111111;
                current = history[index];
                *out++ = current;
                in++;
                continue;
            }
            case 0b01000000: {
                current.r += ((*in & 0b00110000) >> 4) - 2;
                current.g += ((*in & 0b00001100) >> 2) - 2;
                current.b += ((*in & 0b00000011) >> 0) - 2;
                *out++ = current;
                in++;
                goto update_history;
            }
            case 0b10000000: {
                i8 dg = (*in & 0b00111111) - 32;
                i8 drmdg = ((in[1] & 0b11110000) >> 4) - 8;
                i8 dbmdg = ((in[1] & 0b00001111) >> 0) - 8;
                current.r += drmdg + dg;
                current.g += dg;
                current.b += dbmdg + dg;
                *out++ = current;
                in += 2;
                goto update_history;
            }
            case 0b11000000: {
                 // One of these branches could probably be eliminated
                if (*in == 0b11111110) {
                    current.r = in[1];
                    current.g = in[2];
                    current.b = in[3];
                    *out++ = current;
                    in += 4;
                    goto update_history;
                }
                else if (*in == 0b11111111) {
                    current.r = in[1];
                    current.g = in[2];
                    current.b = in[3];
                    current.a = in[4];
                    *out++ = current;
                    in += 5;
                    goto update_history;
                }
                else {
                    u8 len = (*in & 0b00111111) + 1;
                    if (len > out_end - out) goto too_much;
                    do { *out++ = current; } while (--len);
                    in += 1;
                    continue;
                }
            }
        }
        update_history:
        history[hash_pixel(current)] = current;
    }
    if (in != in_end) {
        too_much:
        raise_LoadImageFailed(filename, "Too much data");
    }
    return r;
}

void in::raise_LoadImageFailed (Str filename, Str mess) {
     // TODO: tag
    raise(e_LoadImageFailed, cat(
        "Failed to load image from ", filename, ": ", mess
    ));
}

} using namespace glow;

