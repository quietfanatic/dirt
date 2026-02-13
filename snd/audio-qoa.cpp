#include "audio.h"

#if __SSE4_1__
#include <smmintrin.h>
#endif

#include "../geo/scalar.h"
#include "../uni/endian.h"

namespace snd {
namespace in {

constexpr
u32 samples_to_bytes (u32 samples) {
    return (samples + 19) / 20 * 8;
}
constexpr
u32 samples_per_frame = 20 * 256;
constexpr
u32 bytes_per_frame_channel = 16 + samples_to_bytes(samples_per_frame);

constexpr
u32 samples_to_frames (u32 samples) {
    return (samples + samples_per_frame) / samples_per_frame;
}

constexpr
u32 qoa_filesize (u32 n_channels, u32 n_samples) {
    u32 n_frames = samples_to_frames(n_samples);
    return 8 + n_frames * (8 + 16 * n_channels)
        + samples_to_bytes(n_samples) * n_channels;
}

 // Copied from the reference implementation
 // github.com/phoboslab/qoa/blob/master/qoa.h
constexpr i16 residual_lookup[16][8] = {
    {   1,    -1,    3,    -3,    5,    -5,     7,     -7},
    {   5,    -5,   18,   -18,   32,   -32,    49,    -49},
    {  16,   -16,   53,   -53,   95,   -95,   147,   -147},
    {  34,   -34,  113,  -113,  203,  -203,   315,   -315},
    {  63,   -63,  210,  -210,  378,  -378,   588,   -588},
    { 104,  -104,  345,  -345,  621,  -621,   966,   -966},
    { 158,  -158,  528,  -528,  950,  -950,  1477,  -1477},
    { 228,  -228,  760,  -760, 1368, -1368,  2128,  -2128},
    { 316,  -316, 1053, -1053, 1895, -1895,  2947,  -2947},
    { 422,  -422, 1405, -1405, 2529, -2529,  3934,  -3934},
    { 548,  -548, 1828, -1828, 3290, -3290,  5117,  -5117},
    { 696,  -696, 2320, -2320, 4176, -4176,  6496,  -6496},
    { 868,  -868, 2893, -2893, 5207, -5207,  8099,  -8099},
    {1064, -1064, 3548, -3548, 6386, -6386,  9933,  -9933},
    {1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005},
    {1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336},
};

constexpr u8 residual_i_lookup[17] = {
    7, 7, 7, 5, 5, 3, 3, 1, /* -8..-1 */
    0,                      /*  0     */
    0, 2, 2, 4, 4, 6, 6, 6  /*  1.. 8 */
};

 // These could be u16 except barely the first one lol
static constexpr i32 scalefactor_reciprocals[16] = {
    65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32
};

inline
i32 descale (i32 v, i32 scalefactor) {
    i32 reciprocal = scalefactor_reciprocals[scalefactor];
    i32 r = (v * reciprocal + (1 << 15)) >> 16;
     // Don't return 0 unless we were given 0
    r += ((v > 0) - (v < 0)) - ((r > 0) - (r < 0));
    return r;
}

#if __SSE4_1__
inline
__m128i dot4 (__m128i a, __m128i b) {
     // SSE has dot product instructions for floating point, and partially for
     // i16s, but not for i32s.
    auto r = _mm_mullo_epi32(a, b);
     // https://stackoverflow.com/questions/6996764/fastest-way-to-do-horizontal-sse-vector-sum-or-other-reduction
     // Note, shuffle indexes are right-to-left
    auto s = _mm_shuffle_epi32(r, _MM_SHUFFLE(1,0,3,2));
    r = _mm_add_epi32(r, s);
    s = _mm_shuffle_epi32(r, _MM_SHUFFLE(2,3,0,1));
    r = _mm_add_epi32(r, s);
     // This is fewer instructions and registers and same latency, but more
     // microops apparently.  Since this will be on a hot path, we pick fewer
     // microops.
    //r = _mm_hadd_epi32(r, r);
    //r = _mm_hadd_epi32(r, r);
    return r;
}
#else
i32 dot4 (const i32( &a )[4], const i32( &b )[4]) {
    i32 r = 0;
    for (u32 i = 0; i < 4; i++) {
        r += a[i] * b[i];
    }
    return r;
}
#endif

 // These are stored as i16 in the input, but they're processed as i32
#if __SSE4_1__
 // The main purpose of vectorizing this isn't for immediate throughput (though
 // it probably helps), it's to free up a bunch of integer registers.
struct LMSState {
    __m128i history;
    __m128i weights;
    void init () {
        history = _mm_setzero_si128();
        weights = _mm_set_epi32(2 << 13, -1 << 13, 0, 0);
    }
    const u8* read (const u8* in) {
         // Values are right-to-left!
        const __m128i swap16 = _mm_set_epi8(
            14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1
        );
        auto big = _mm_loadu_si128((const __m128i*)in);
        auto lil = _mm_shuffle_epi8(big, swap16);
        history = _mm_cvtepi16_epi32(lil);
        lil = _mm_srli_si128(lil, 8); // shifts by bytes, not bits!
        weights = _mm_cvtepi16_epi32(lil);
        return in + 16;
    }
    u8* write (u8* out) {
         // SSE4 doesn't have any instructions to narrow with wraparound, so we
         // have to do some weird tricks.  We could probably just use signed
         // saturation, but it's not proven that the weights stay in i16 range,
         // and the reference encoder just &s the weights with 0xffff instead of
         // saturating them.
         //
         // Fortunately, if we're twiddling bytes around, we can do the
         // narrowing and the endian swap at the same time.  We really don't
         // need to optimize this so much, it's not critical, but here we are.
        const __m128i narrowandswap = _mm_set_epi8(
            0,0,0,0,0,0,0,0,12,13,8,9,4,5,0,1
        );
        auto w = _mm_shuffle_epi8(weights, narrowandswap);
        auto h = _mm_shuffle_epi8(history, narrowandswap);
        auto repr = _mm_unpacklo_epi64(h, w); // Pun Pickle Cutie Cue
        _mm_storeu_si128((__m128i*)out, repr);
        return out + 16;
    }
    __m128i predict () {
        auto pred = dot4(history, weights);
        return _mm_srai_epi32(pred, 13);
    }
    void update (__m128i sample, __m128i residual) {
        auto deltas = _mm_srai_epi32(residual, 4);
        deltas = _mm_shuffle_epi32(deltas, 0);
         // Can't use _mm_sign_epi32 by itself because it outputs 0 when its
         // input is 0, but we are supposed to treat 0 as positive.
        auto ones = _mm_set_epi32(1,1,1,1);
        auto signs = _mm_or_si128(history, ones);
        deltas = _mm_sign_epi32(deltas, signs);
        weights = _mm_add_epi32(weights, deltas);
        history = _mm_alignr_epi8(sample, history, 4);
    }
};
#else
struct LMSState {
    i32 history [4];
    i32 weights [4];
    void init () {
        for (u32 i = 0; i < 4; i++) {
            history[i] = 0;
        }
        weights[0] = 0;
        weights[1] = 0;
        weights[2] = -1 << 13;
        weights[3] = 2 << 13;
    }
    const u8* read (const u8* in) {
        for (u32 i = 0; i < 4; i++) {
            history[i] = i16(read_u16be(in));
            in += 2;
        }
        for (u32 i = 0; i < 4; i++) {
            weights[i] = i16(read_u16be(in));
            in += 2;
        }
        return in;
    }
    u8* write (u8* out) {
        for (u32 i = 0; i < 4; i++) {
            write_u16be(out, history[i]);
            out += 2;
        }
        for (u32 i = 0; i < 4; i++) {
            write_u16be(out, weights[i]);
            out += 2;
        }
        return out;
    }
    i32 predict () {
        i32 pred = dot4(history, weights);
        return pred >> 13;
    }
    void update (i32 sample, i32 residual) {
        int delta = residual >> 4;
        for (u32 i = 0; i < 4; i++) {
            weights[i] += history[i] < 0 ? -delta : delta;
        }
        for (u32 i = 0; i < 3; i++) {
            history[i] = history[i+1];
        }
        history[3] = sample;
    }
};
#endif

static
void qoa_decode_slice (
    LMSState&__restrict state, i16*__restrict out,
    const u8*__restrict in, u32 n_channels
) {
    u64 slice = read_u64be(in);
    u32 scalefactor_i = slice >> 60;
    auto& table = residual_lookup[scalefactor_i];
    slice <<= 4;
    auto out_end = out + (20 * n_channels);
    #pragma GCC unroll 0
    do {
         // Predict
        auto pred = state.predict();
         // Decompress
        u32 residual_i = slice >> 61;
#if __SSE4_1__
        auto residual = _mm_cvtsi32_si128(table[residual_i]);
        auto sample = _mm_add_epi32(pred, residual);
         // Saturate
        sample = _mm_packs_epi32(sample, sample);
        sample = _mm_cvtepi16_epi32(sample);
         // Store
        *out = _mm_cvtsi128_si32(sample);
#else
        i32 residual = table[residual_i];
        i32 sample = pred + residual;
         // Saturate
        if (u32(sample + 32768) > 65535) [[unlikely]] {
            sample = 32767 ^ (sample >> 31);
        }
         // Store
        *out = sample;
#endif
         // Update predictor
        state.update(sample, residual);
         // Move along
        out += n_channels;
        slice <<= 3;
    } while (out < out_end);
}

struct DecodeFrameReturn {
    i16* out;
    const u8* in;
};

static
DecodeFrameReturn qoa_decode_frame (
    i16*__restrict out, const u8*__restrict in, u32 n_channels
) {
    LMSState states [8];
    u32 n_samples = read_u16be(in + 4);
    in += 8; // header already validated
    expect(n_channels > 0 && n_channels <= 8);
    #pragma GCC unroll 0
    for (u32 c = 0; c < n_channels; c++) {
        in = states[c].read(in);
    }
     // Input is interleaved per-slice, output per-sample.
    expect(n_samples > 0);
    #pragma GCC unroll 0
    for (i32 s = n_samples; s > 0; s -= 20) {
        #pragma GCC unroll 0
        for (u32 c = 0; c < n_channels; c++) {
            qoa_decode_slice(states[c], out, in, n_channels);
            out += 1;
            in += 8;
        }
        out += 19 * n_channels;
    }
    return {out, in};
}

NOINLINE static
void qoa_decode_frames (
    i16*__restrict out, const u8*__restrict in,
    u32 n_frames, u32 n_channels
) {
    do {
        auto r = qoa_decode_frame(out, in, n_channels);
        out = r.out; in = r.in;
        n_frames -= 1;
    } while (n_frames);
}

} using namespace in;

UniqueAudio audio_from_blob_qoa (Slice<u8> blob, Str filename) {
    const u8* in = blob.begin();
    const u8* in_end = blob.end();
    UniqueAudio r;
    Str mess;
    { // Validate file header
        if (in_end - in < 16 || read_u32le(in) != read_u32le("qoaf")) {
            mess = "File is not QOA format"; goto bad;
        }
        r.n_samples = read_u32be(in + 4);
        if (!r.n_samples) {
            mess = "File doesn't specify number of samples"; goto bad;
        }
    }
    { // Read first frame header
        u64 first_frame_header = read_u64be(in + 8);
        r.n_channels = first_frame_header >> 56;
        if (r.n_channels == 0 || r.n_channels > 8) {
            mess = "Unsupported channel count"; goto bad;
        }
        r.sample_rate = first_frame_header >> 32 & 0xffffff;
        if (!r.sample_rate) {
            mess = "Sample rate cannot be zero"; goto bad;
        }
    }
    { // More validation
        u32 n_frames = samples_to_frames(r.n_samples);
        u32 max_frame_size = 8 + bytes_per_frame_channel * r.n_channels;
        u64 expected_size = qoa_filesize(r.n_channels, r.n_samples);
        if (in + expected_size != in_end) {
            mess = "File is wrong size for number of channels and samples"; goto bad;
        }
        in += 8;
        auto frame = in;
         // Validate all frame headers
        u32 samples_left = r.n_samples;
        do {
            u64 header = read_u64be(frame);
            u32 frame_n_channels = header >> 56;
            if (frame_n_channels != r.n_channels) {
                mess = "Inconsistent n_channels"; goto bad;
            }
            u32 frame_sample_rate = header >> 32 & 0xffffff;
            if (frame_sample_rate != r.sample_rate) {
                mess = "Inconsistent sample rate"; goto bad;
            }
            u32 frame_n_samples = header >> 16 & 0xffff;
            u32 frame_size = header & 0xffff;
            if (frame + max_frame_size <= in_end) {
                 // Expect non-last frame to be max size
                if (frame_n_samples != samples_per_frame) {
                    mess = "Nonfinal frame has incorrect n_samples"; goto bad;
                }
                if (frame_size != max_frame_size) {
                    mess = "Nonfinal frame has incorrect size"; goto bad;
                }
            }
            else {
                if (frame_n_samples != samples_left) {
                    mess = "Final frame has incorrect n_samples"; goto bad;
                }
                if (frame_size != in_end - frame) {
                    mess = "Final frame has incorrect size"; goto bad;
                }
            }
            frame += max_frame_size;
            samples_left -= samples_per_frame;
        } while (frame < in_end);
         // Allocate (add some overhead because the decoder doesn't know how to
         // stop in the middle of a slice)
        usize size = r.n_channels * ((r.n_samples + 19) / 20 * 20);
         // Plus the normal 4 overhead for UniqueAudio.
        r.samples = new i16 [size + 4];
        for (u32 i = 0; i < 4; i++) {
            r.samples[size + i] = 0;
        }
         // Decode
        qoa_decode_frames(r.samples, in, n_frames, r.n_channels);
    }
    return r;
    bad: raise_LoadAudioFailed(filename, mess);
}

namespace in {

struct EncodeFrameReturn {
    u8* out;
    const i16* in;
};

struct QOAEncoder {
    LMSState states [8];
    u32 n_channels;
    u32 samples_left; // Counts down
    u32 sample_rate;
    u32 last_scalefactor = 0;

    void encode_slice (
        u8*__restrict out, const i16*__restrict in,
        LMSState&__restrict channel_state, u32 slice_samples
    ) {
         // Search for best scale factor
        u64 best_rank = -1;
        u32 best_scalefactor;
        LMSState best_state;
        u64 best_slice;
        #pragma GCC unroll 0
        for (u32 sfi = 0; sfi < 16; sfi++) {
             // Start with previous scalefactor like reference encoder
            u32 scalefactor = (last_scalefactor + sfi) & 15;
            auto& table = residual_lookup[scalefactor];
            u64 slice = u64(scalefactor) << 60;
            u32 residual_offset = 57;
            u64 rank = 0;
            LMSState state = channel_state;
            expect(slice_samples > 0 && slice_samples <= 20);
            #pragma GCC unroll 0
            for (u32 i = 0; i < slice_samples; i++) {
                 // Predict
                auto prediction = state.predict();
#if __SSE4_1__
                 // Compress
                i32 in_sample = in[i * n_channels];
                i32 in_residual = in_sample - _mm_cvtsi128_si32(prediction);
                i32 descaled = descale(in_residual, scalefactor);
                i32 clamped = descaled = geo::clamp(descaled, -8, 8);
                u32 residual_i = residual_i_lookup[clamped + 8];
                 // Decompress
                auto out_residual = _mm_cvtsi32_si128(table[residual_i]);
                auto out_sample = _mm_add_epi32(prediction, out_residual);
                out_sample = _mm_packs_epi32(out_sample, out_sample);
                out_sample = _mm_cvtepi16_epi32(out_sample);
                 // Calculate error
                i64 error = in_sample - _mm_cvtsi128_si32(out_sample);
                auto wpen = dot4(state.weights, state.weights);
                i32 weights_penalty = _mm_cvtsi128_si32(wpen);
#else
                 // Compress
                i32 in_sample = in[i * n_channels];
                i32 in_residual = in_sample - prediction;
                i32 descaled = descale(in_residual, scalefactor);
                i32 clamped = descaled = geo::clamp(descaled, -8, 8);
                u32 residual_i = residual_i_lookup[clamped + 8];
                 // Decompress
                i32 out_residual = table[residual_i];
                i32 out_sample = prediction + out_residual;
                if ((u32)(out_sample + 32768) > 65535) [[unlikely]] {
                    out_sample = 32767 ^ (out_sample >> 31);
                }
                 // calculate error
                i64 error = in_sample - out_sample;
                 // Penalty for weights growing too large (this is optional but
                 // the reference encoder does it).
                i32 weights_penalty = dot4(state.weights, state.weights);
#endif
                weights_penalty = geo::max((weights_penalty >> 18) - 0x8ff, 0);
                 // Rank this scalefactor
                rank += error * error + weights_penalty * weights_penalty;
                 // If we're already worse than the best, bail out.
                if (rank > best_rank) break;
                state.update(out_sample, out_residual);
                slice |= u64(residual_i) << residual_offset;
                residual_offset -= 3;
            }
            if (rank < best_rank) {
                best_rank = rank;
                best_scalefactor = scalefactor;
                best_state = state;
                best_slice = slice;
            }
        }
        expect(best_rank < u64(-1));
        last_scalefactor = best_scalefactor;
        channel_state = best_state;
        write_u64be(out, best_slice);
    }

    EncodeFrameReturn encode_frame (
        u8*__restrict out, const i16*__restrict in
    ) {
         // Write header
        u32 frame_samples = geo::min(samples_left, samples_per_frame);
        u32 frame_slices = (frame_samples + 19) / 20;
        u32 frame_size = 8 + (16 + frame_slices * 8) * n_channels;
        u64 header = u64(n_channels) << 56
                   | u64(sample_rate) << 32
                   | frame_samples << 16
                   | frame_size;
        write_u64be(out, header);
        out += 8;
         // Write predictor states
        expect(n_channels > 0 && n_channels <= 8);
        #pragma GCC unroll 0
        for (u32 c = 0; c < n_channels; c++) {
            out = states[c].write(out);
        }
         // Now encode slices
        expect(frame_slices);
        #pragma GCC unroll 0
        while (frame_slices) {
            u32 slice_samples = geo::min(samples_left, 20u);
            for (usize c = 0; c < n_channels; c++) {
                encode_slice(out, in, states[c], slice_samples);
                out += 8;
                in += 1;
            }
            in += (slice_samples - 1) * n_channels;
            samples_left -= slice_samples;
            frame_slices -= 1;
        }
        return {out, in};
    }

    NOINLINE
    void encode_file (
        u8*__restrict out, const i16*__restrict in,
        u8* out_end, const i16* in_end
    ) {
        #pragma GCC unroll 0
        for (u32 c = 0; c < n_channels; c++) {
            states[c].init();
        }
        std::memcpy(out, "qoaf", 4);
        write_u32be(out+4, samples_left);
        out += 8;
        expect(samples_left);
        #pragma GCC unroll 0
        while (samples_left) {
            auto r = encode_frame(out, in);
            out = r.out; in = r.in;
            expect(out <= out_end);
            expect(in <= in_end);
        }
    }
};

} // in

UniqueArray<u8> audio_to_blob_qoa (const UniqueAudio& au, Str) {
    usize file_size = qoa_filesize(au.n_channels, au.n_samples);
    auto r = UniqueArray<u8>(Uninitialized(file_size));
    QOAEncoder encoder;
    encoder.n_channels = au.n_channels;
    encoder.samples_left = au.n_samples;
    encoder.sample_rate = au.sample_rate;
    encoder.encode_file(
        r.begin(), au.samples,
        r.end(), au.samples + au.size()
    );
    return r;
}

} // snd

