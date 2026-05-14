#include "image.h"

#include "../uni/endian.h"
#include "../uni/io.h"

namespace glow {

namespace in {

[[noreturn, gnu::cold]] static
void raise_LoadImageFailed (StaticString mess) {
    raise(e_LoadImageFailed, mess);
}

[[noreturn, gnu::cold]] static
void raise_ImageTooLarge (IVec size) {
    raise(e_SaveImageFailed, cat(
        "Image is too large (", size.x, 'x', size.y, " = ", area(size), " > 400000000)"
    ));
}

} using namespace in;

 // If this returns u8, GCC will try to vectorize this badly for some reason.
static constexpr
u32 hash_pixel (u8 r, u8 g, u8 b, u8 a) {
    return (r*3 + g*5 + b*7 + a*11) & 63;
}

///// DECODER

 // Returns 0 if successful, + if too much input, - if too little input.
NOINLINE static
int decode_qoi (
    RGBA8*__restrict out, RGBA8* out_end,
    const u8*__restrict in, const u8* in_end
) noexcept {
    RGBA8 history [64] = {};
     // We're primarily keeping the pixel coalesced in one register, because the
     // two most common ops (index and run) only care about the coalesced form.
    RGBA8 px = {0, 0, 0, 255};
     // However, predeclare the individual registers so we can use them across
     // the goto.  These have to be u8 because they are specced to wrap around.
    u8 r, g, b, a;
     // Special semi- or fully-paranoid tweak to allow an optimization. [1]
#ifdef GLOW_DECODE_QOI_PARANOID
    bool start_run = *in >= 0b11000000 && *in < 0b11111110;
    history[59].a = -start_run;
#else
    history[59].a = 255;
#endif
    while (out < out_end && in < in_end) {
         // Ordering these by rough likelihood for a pixel-art game with a flat
         // art style.
        if (*in < 0b01000000) [[likely]] { // QOI_OP_INDEX
            px.repr = history[*in].repr;
            (out++)->repr = px.repr;
#ifdef GLOW_DECODE_QOI_PARANOID
            if (px.repr == 0) [[unlikely]] history[0].repr = px.repr;
#endif
            in += 1;
            continue;
        }
        else if (*in >= 0b11000000) {
            if (*in < 0b11111110) [[likely]] { // QOI_OP_RUN
                 // Most runs are 1, 2, or 62.  We could special-case the
                 // smallest runs, but I don't think it's worth the extra branch
                 // mispredictions.
                u32 len = u32(*in) - 0b11000000 + 1;
                RGBA8* real_end = out + len;
                 // If we have extra room, we can round up the run length to a
                 // multiple of 4 so the compiler can vectorize it without a
                 // bunch of tail-cleanup branches.  It'd be simpler to
                 // explicitly do 4 at a time, which could skip the rounding up
                 // calculation, but then the compiler always makes a worse loop
                 // (it seems to invent an integer loop count variable instead
                 // of comparing pointers like we asked it to).
                 //
                 // Note that this two-loop setup is still more compact what the
                 // compiler would generate if left to its own devices.  I wish
                 // I had RISC-V vectors.
                RGBA8* optimistic_end = out + ((len + 3) & ~3);
                if (optimistic_end <= out_end) {
                    expect(out < optimistic_end);
                    while (out < optimistic_end) {
                        (out++)->repr = px.repr;
                    }
                    out = real_end;
                }
                else [[unlikely]] {
                     // No extra room, so we have to be precise.
                    if (real_end > out_end) [[unlikely]] break;
                    expect(out < real_end);
                     // This should happen at most once or twice so don't unroll
                     // or vectorize it.
                    #pragma GCC unroll 0
                    #pragma GCC novector
                    while (out < real_end) {
                        (out++)->repr = px.repr;
                    }
                }
                in += 1;
                continue;
            }
            else { // QOI_OP_RGB or QOI_OP_RGBA
                 // Some hacky hacks to make this path branchless
                RGBA8 new_px;
                std::memcpy(&new_px.repr, in+1, 4);
                if (*in == 0b11111110) new_px.a = px.a;
                in += *in - 0b11111110 + 4; // Add 4 or 5
                px = new_px;
                r = px.r; g = px.g; b = px.b; a = px.a;
            }
        }
        else {
            if (*in >= 0b10000000) [[likely]] { // QOI_OP_LUMA
                i8 dg = *in - 0b10000000 - 32;
                i8 dr_g = ((in[1] & 0b11110000) >> 4) - 8;
                i8 db_g = ((in[1] & 0b00001111) >> 0) - 8;
                r = px.r + dr_g + dg;
                g = px.g + dg;
                b = px.b + db_g + dg;
                a = px.a;
                in += 2;
            }
            else { // QOI_OP_DIFF
                r = px.r + ((*in & 0b00110000) >> 4) - 2;
                g = px.g + ((*in & 0b00001100) >> 2) - 2;
                b = px.b + ((*in & 0b00000011) >> 0) - 2;
                a = px.a;
                in += 1;
            }
            px = {r, g, b, a};
        }
        (out++)->repr = px.repr;
        history[hash_pixel(r, g, b, a)].repr = px.repr;
    }
     // out and in should be exhausted at the same time.
    return out < out_end ? 1 : in_end - in;
}

UniqueImage image_from_blob_qoi (Slice<u8> blob) {
    if (blob.size() < 14 + 8) raise_LoadImageFailed("File is too short");
    if (Str(blob.slice(0, 4)) != "qoif") raise_LoadImageFailed("File is not QOI format");
    if (Str(blob.slice(blob.size()-8)) != "\x00\x00\x00\x00\x00\x00\x00\x01") {
        raise_LoadImageFailed("QOI file doesn't end properly");
    }
    const u8* in = blob.begin();
    const u8* in_end = blob.end();
    u32 width = read_u32be(in + 4);
    u32 height = read_u32be(in + 8);
    u64 len = width * height;
    if (len > 400000000) raise_LoadImageFailed("Image is too large");
     // Ignore channels and colorspace for now.

    UniqueImage r (IVec(width, height));
    RGBA8* out = r.pixels;
    RGBA8* out_end = out + len;
    in += 14;
    in_end -= 8;

    int res = decode_qoi(out, out_end, in, in_end);
    if (res) {
        raise_LoadImageFailed(
            res > 0 ? StaticString("Too much data") : StaticString("Not enough data")
        );
    }

    return r;
}

///// ENCODER

NOINLINE
u8* encode_qoi (u8*__restrict out, const ImageView&__restrict img) noexcept {
    expect(img.size.x >= 0 && img.size.y >= 0);
    if (img.size.x == 0 || img.size.y == 0) [[unlikely]] return out;
     // Set up pointer-walking infrastructure
    const RGBA8*__restrict in = img.pixels;
    const RGBA8* in_end = in + img.size.x;
    const RGBA8* in_end_end = in_end + img.stride * img.size.y;
     // Cut out one branch if the image is contiguous
    if (img.contiguous()) {
        in_end = in_end_end - img.stride;
    }
     // Encoder state
    RGBA8 history [64] = {};
    RGBA8 last = {0, 0, 0, 255};

    loop:
     // First check for runs
    if (in->repr == last.repr) {
        start_run:
        u32 run = 0b11000000; // QOI_OP_RUN (this encodes length 1)
        continue_run:
         // Bump pointer
        in += 1;
        if (in == in_end) [[unlikely]] {
             // Bump pointer a little harder
            in_end += img.stride;
            if (in_end == in_end_end) {
                 // Bumped pointer so hard we ran out of input
                *out++ = run;
                return out;
            }
            in += img.stride - img.size.x;
        }
         // Run continues?
        if (in->repr == last.repr) {
            if (run == 0b11111101) {
                 // Run is already at max length of 62
                *out++ = run;
                 // Start new run at length 1
                goto start_run;
            }
            run += 1;
            goto continue_run;
        }
         // Run done
        *out++ = run;
         // fall through
    }
     // Now either read or write history
    u32 hash = hash_pixel(in->r, in->g, in->b, in->a);
    if (history[hash].repr == in->repr) {
        *out++ = 0b00000000 | hash; // QOI_OP_INDEX
        goto next;
    }
    else history[hash].repr = in->repr;
     // New pixel.  How shall we encode it?
    if (in->a == last.a) {
         // See if we can cram the delta encoding into one byte.  All of these
         // values are specced to wrap around at 8 bits.  We can range check all
         // of them at once by oring them together.
        u8 dr = in->r - last.r + 2;
        u8 dg = in->g - last.g + 2;
        u8 db = in->b - last.b + 2;
        if (!((dr | dg | db) & 0b11111100)) {
            *out++ = 0b01000000 | (dr << 4) | (dg << 2) | db; // QOI_OP_DIFF
            goto next;
        }
         // Okay try the two-byte encoding.  Even if the bitfields have different
         // sizes, we can still check for overflow with only one branch.
        u8 dr_g = (in->r - last.r) - (in->g - last.g) + 8;
        u8 dg2 = in->g - last.g + 32;
        u8 db_g = (in->b - last.b) - (in->g - last.g) + 8;
        if (!((dg2 & 0b11000000) | ((dr_g | db_g) & 0b11110000))) {
            *out++ = 0b10000000 | dg2; // QOI_OP_LUMA
            *out++ = (dr_g << 4) | db_g;
            goto next;
        }
         // Nope, this pixel is too weird to compress
        *out++ = 0b11111110; // QOI_OP_RGB
        *out++ = in->r;
        *out++ = in->g;
        *out++ = in->b;
        *out = in->a; // Do a 32-bit store
        goto next;
    }
    else {
         // If the alpha changes we have to write the full RGBA pixel
        *out++ = 0b11111111; // QOI_OP_RGBA
        *out++ = in->r;
        *out++ = in->g;
        *out++ = in->b;
        *out++ = in->a;
        goto next;
    }

    next:
     // Don't forget this!
    last.repr = in->repr;
     // Move pointer to the right
    in += 1;
    if (in == in_end) [[unlikely]] {
         // Hit right edge so start next row
        in_end += img.stride;
        if (in_end == in_end_end) {
             // Hit bottom-right corner so we're done
            return out;
        }
        in += img.stride - img.size.x;
    }
    goto loop;
}

UniqueArray<u8> image_to_blob_qoi (const ImageView& img) {
    expect(img.size.x >= 0 && img.size.y >= 0);
    usize len = area(img.size);
    if (len > 400000000) raise_ImageTooLarge(img.size);
     // Worst case 5 bytes per pixel + 14 byte header + 8 byte footer
    u8* buf = SharableBuffer<u8>::allocate(5 * len + 22);
    std::memcpy(buf, "qoif", 4);
    write_u32be(buf+4, img.size.x);
    write_u32be(buf+8, img.size.y);
     // channels: RGBA.  TODO: propagate this.
    buf[12] = 4;
     // colorspace: sRGB.  I don't think we're really using sRGB, but SAIL's
     // QOI loader refuses to load it unless this is 0.
    buf[13] = 0;
    u8* end = encode_qoi(buf + 14, img);
    write_u64be(end, 1);
    end += 8;
    UniqueArray<u8> r;
    expect(u32(end - buf) == end - buf);
    r.impl = {u32(end - buf), buf};
    return r;
}

} using namespace glow;

///// TESTS

#ifndef TAP_DISABLE_TESTS
#include "../ayu/resources/resource.h"
#include "../tap/tap.h"
#include "colors.h"
#include "test/test-environment.h"

static tap::TestSet tests ("dirt/glow/image-qoi", []{
    using namespace tap;
    using namespace geo;

    test::TestEnvironment env;

    auto path = ayu::resource_filepath(iri::IRI("test:/testcard_rgba.qoi"));
    auto input = blob_from_file(path);
    UniqueImage img = image_from_blob(input);
    auto output = image_to_blob(img);
    if (!is(input, output, "QOI Decoder and encoder agree")) {
        auto outpath = ayu::resource_filepath(iri::IRI("test:/TESTFAIL-testcard_rgba.qoi"));
        blob_to_file(output, outpath);
    }

    done_testing();
});

#endif

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
 // an initial run, we'll just set the history entry always.  In theory, an
 // encoder COULD emit a QOI_OP_INDEX that uses this history entry, assuming it
 // has been initialized to {0,0,0,0}.  That would be weird--borderline
 // malicious--when the properly hashed entry 0 is right there.  I suppose I
 // could see a hyper-aggressively compressing encoder doing so, if it's filled
 // the proper history entry with something else, so it gets its {0,0,0,0} from
 // an improper entry.  And if it does that, then according to the spec even
 // QOI_OP_INDEX must update the history, meaning that indexing an improper
 // entry for {0,0,0,0} would set the proper entry to {0,0,0,0}, which would
 // threaten our optimization even more.
 //
 // So I'm just gonna assume that will never happen.  If an encoder wants to
 // spend that much effort to occasionally save a pittance of bytes, it should
 // be spending those cycles on DEFLATE or something instead.
 //
 // Alternatively, you can #define GLOW_DECODE_QOI_PARANOID to make sure all
 // possible scenarios are covered, at a slight performance loss.
