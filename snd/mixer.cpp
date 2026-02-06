#include "mixer.h"

#include <cmath>
#include "../uni/indestructible.h"

#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

namespace snd {
using namespace uni;

namespace in {

void raise_VoiceUnsupported (StaticString mess) {
    raise(e_VoiceUnsupported, mess);
}

NOINLINE
void grow_voices (UniqueArray<Voice>& vs, u32 channel) {
    require(channel <= Mixer::highest_channel);
    vs.reserve_plenty(channel + 1);
    expect(channel + 1 <= vs.capacity());
    vs.grow(channel + 1);
}

} using namespace in;

void Mixer::play (const Voice& voice, u32 channel) {
    if (voice.audio->n_channels != 1 && voice.audio->n_channels != 2) {
        raise_VoiceUnsupported("Audio n_channels must be 1 or 2");
    }
    if (channel >= voices.size()) grow_voices(voices, channel);
    voices[channel] = voice;
}

u32 Mixer::play_on_free_channel (const Voice& voice, u32 minimum) {
    if (voice.audio->n_channels != 1 && voice.audio->n_channels != 2) {
        raise_VoiceUnsupported("Audio n_channels must be 1 or 2");
    }
    u32 channel;
    for (channel = minimum; channel < voices.size(); channel += 1) {
        if (!voices[channel].audio) goto found;
    }
    grow_voices(voices, channel);
    found:
    voices[channel] = voice;
    return channel;
}

void Mixer::stop (u32 channel) {
    if (channel < voices.size()) {
        voices[channel].audio = null;
    }
}

void Mixer::stop_all () {
    for (auto& v : voices) {
        v.audio = null;
    }
}

using StereoFloat = float[2];

 // This is awkward because we're combining fixed-point and floating-point math,
 // and also merging the lerp weights with volume scaling.  The compiler isn't
 // allowed to do much floating point optimization, so we have to do it by hand.
 //
 // We're using linear interpolation, which can lose some of the higher
 // frequencies, but given we're working with 40khz+ audio, it shouldn't really
 // be noticable.
[[gnu::hot]]
void Mixer::mix (
    StereoFloat*__restrict out, u32 out_len, u32 out_rate
) {
    for (auto o = out; o < out + out_len; o++) {
        (*o)[0] = 0;
        (*o)[1] = 0;
    }
    for (auto& v : voices) {
        if (!v.audio) continue;
        u32 out_i = 0;
        const i16*__restrict in = v.audio->samples;
         // Don't forget to write this back after looping!
        i64 in_pos = v.position;
        expect(in_pos >= 0);
         // Figure out how much input we can get before doing something weird
        i64 in_end = v.loop_end < 0 ? v.n_chronons() : v.loop_end;
         // How fast to consume the input compared to the output.
        i64 in_speed = (i64(v.speed) << 8) * v.audio->sample_rate / out_rate;
        expect(in_speed >= 0);
         // This is the "one" value for the first lerp weight.
        float w_add = 0x1'0000'0000;
         // This simultaneously descales the lerp weights from 0:32,
         // preemptively descales the to-be-multiplied samples from 0:15,
         // and multiplies the volume in.
        float w_mul = (1.f/0x8000'0000'0000) * v.volume;
        process:
        if (v.audio->n_channels == 1) {
             // Mono
            expect(out_len > 0);
            for (; out_i < out_len; out_i++) {
                if (in_pos >= in_end) goto stop_or_loop;
                u32 in_i = u64(in_pos) >> 32;
                float t = u32(in_pos);
#if __SSE4_1__
                auto w_adds = _mm_set_ps(0, 0, 0, -w_add);
                auto w_muls = _mm_set_ps(0, 0, w_mul, -w_mul);

                auto ws = _mm_set1_ps(t);
                ws = _mm_add_ps(ws, w_adds);
                ws = _mm_mul_ps(ws, w_muls);

                auto s16s = _mm_loadu_si32((u32*)(in + in_i));
                auto ss = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(s16s));

                ss = _mm_mul_ps(ss, ws);
                ss = _mm_add_ps(ss, _mm_shuffle_ps(ss, ss, _MM_SHUFFLE(2,3,0,1)));
                 // Not sure if this cast causes a domain-switch delay but
                 // casting floats to doubles seems less likely to than floats
                 // to i32s.  If the delay does happen it's still worth it.
                auto acc = _mm_castpd_ps(_mm_load_sd((double*)(out+out_i)));
                ss = _mm_add_ps(ss, acc);
                _mm_store_sd((double*)(out+out_i), _mm_castps_pd(ss));
#else
                float w0 = (t - w_add) * -w_mul;
                float w1 = t * w_mul;
                float s0 = in[in_i];
                float s1 = in[in_i+1];
                float s = s0 * w0 + s1 * w1;
                out[out_i][0] += s;
                out[out_i][1] += s;
#endif
                in_pos += in_speed;
            }
        }
        else if (v.audio->n_channels == 2) {
             // Stereo
            expect(out_len > 0);
            for (; out_i < out_len; out_i++) {
                if (in_pos >= in_end) goto stop_or_loop;
                u32 in_i = u64(in_pos) >> 32;
                float t = u32(in_pos);
#if __SSE4_1__
                 // (right-to-left) r1 l1 r0 l0
                auto w_adds = _mm_set_ps(0, 0, -w_add, -w_add);
                auto w_muls = _mm_set_ps(w_mul, w_mul, -w_mul, -w_mul);

                auto ws = _mm_set1_ps(t);
                ws = _mm_add_ps(ws, w_adds);
                ws = _mm_mul_ps(ws, w_muls);

                auto s16s = _mm_loadu_si64((u64*)(in + in_i * 2));
                auto ss = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(s16s));

                ss = _mm_mul_ps(ss, ws);
                ss = _mm_add_ps(ss, _mm_shuffle_ps(ss, ss, _MM_SHUFFLE(1,0,3,2)));

                auto acc = _mm_castpd_ps(_mm_load_sd((double*)(out+out_i)));
                ss = _mm_add_ps(ss, acc);
                _mm_store_sd((double*)(out+out_i), _mm_castps_pd(ss));
#else
                float w0 = (t - w_add) * -w_mul;
                float w1 = t * w_mul;
                float l0 = in[in_i*2];
                float r0 = in[in_i*2+1];
                float l1 = in[in_i*2+2];
                float r1 = in[in_i*2+3];
                float l = l0 * w0 + l1 * w1;
                float r = r0 * w0 + r1 * w1;
                out[out_i][0] += l;
                out[out_i][1] += r;
#endif
                in_pos += in_speed;
            }
        }
        else never();
        v.position = in_pos;
        continue;
        stop_or_loop: {
            if (v.loop_end < 0) {
                v.audio = null;
                continue;
            }
            else {
                in_pos -= v.loop_end - v.loop_start;
                goto process;
            }
        }
    }
}

} using namespace snd;

#ifndef TAP_DISABLE_TESTS
#include "../ayu/reflection/describe-standard.h"
#include "../ayu/traversal/to-tree.h"
#include "../tap/tap.h"
#include "../uni/io.h"
#include "../whereami/whereami.h"

static tap::TestSet tests ("dirt/snd/mixer", []{
    using namespace tap;

    int len = wai_getExecutablePath(null, 0, null);
    auto dir = UniqueString(Uninitialized(len));
    wai_getExecutablePath(dir.data(), len, null);
    while (dir.back() != '/') dir.pop_back();
    encat(dir, "res/dirt/snd/test/");

    UniqueAudio in0 (1, 2048, 48000);
    UniqueAudio in1 (2, 2048, 44100);
    float out [512][2];
    auto out_bytes = MutSlice<u8>((u8*)out, sizeof(out));

    for (u32 i = 0; i < 2048; i++) {
        in0.samples[i] = i;
        in1.samples[i*2] = i;
        in1.samples[i*2+1] = -i;
    }

    Voice v0;
    v0.audio = &in0;
    Voice v1;
    v1.audio = &in1;
    v1.loop_start = 0; // TODO: test non-zero loop_start
    v1.loop_end = 520 * Voice::chronons_per_sample;

    Mixer mixer;
    mixer.play_on_free_channel(v0);
    mixer.play_on_free_channel(v1);

    mixer.mix(out, 512, 48000);

    auto want = [](float v0, float v1) {
        return (v0 + v1 * (44100.f / 48000.f)) * (1/32767.f);
    };

    is(out[0][0], 0.f, "First buffer");
    is(out[0][1], 0.f);
    about(out[1][0], want(1, 1));
    about(out[1][1], want(1, -1));
    about(out[399][0], want(399, 399));
    about(out[399][1], want(399, -399));
    about(out[511][0], want(511, 511));
    about(out[511][1], want(511, -511));

    if (failures()) {
        string_to_file(
            ayu::show(&out, ayu::PrintOptions::Pretty),
            cat(dir, "mixer-out-0.ayu")
        );
    }

    mixer.mix(out, 512, 48000);

    about(out[0][0], want(512, 512), "Second buffer");
    about(out[0][1], want(512, -512));
    about(out[53][0], want(565, 565));
    about(out[53][1], want(565, -565));
    about(out[54][0], want(566, 0), "After loop");
    about(out[54][1], want(566, 0));
    about(out[120][0], want(632, 66));
    about(out[120][1], want(632, -66));

    if (failures()) {
        string_to_file(
            ayu::show(&out, ayu::PrintOptions::Pretty),
            cat(dir, "mixer-out-1.ayu")
        );
    }

    done_testing();
});

#endif
