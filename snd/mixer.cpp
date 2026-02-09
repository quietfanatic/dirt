#include "mixer.h"

#include <cmath>
#include "../geo/scalar.h"
#include "../uni/indestructible.h"

#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

namespace snd {
using namespace uni;

namespace in {

NOINLINE
void grow_voices (UniqueArray<VoiceImp>& vs, u32 channel) {
    require(channel <= Mixer::highest_channel);
    vs.reserve_plenty(channel + 1);
    expect(channel + 1 <= vs.capacity());
    vs.grow(channel + 1);
}

} using namespace in;

void VoiceSpec::validate () const {
    StaticString mess;
    if (!audio) { mess = "audio is null"; goto bad; }
    if (audio->n_channels != 1 && audio->n_channels != 2) {
        mess = "audio->n_channels isn't 1 or 2"; goto bad;
    }
    if (audio->n_channels > u32(i32(geo::GINF))) {
        mess = "audio->samples is too large"; goto bad;
    }
     // Use positive comparisons to reject NANs
    if (volume >= 0 && volume <= 16) { }
    else { mess = "volume out of range"; goto bad; }
    if (speed >= 0 && speed <= 16) { }
    else { mess = "speed out of range"; goto bad; }
    if (loop_start >= 0 && loop_start <= n_seconds(audio)) { }
    else { mess = "loop_start out of range"; goto bad; }
     // Except this one, NAN is valid for loop_end
    if (loop_end < 0) { mess = "loop_end out of range"; goto bad; }
    if (loop_end < loop_start) {
        mess = "loop_end is before loop_start"; goto bad;
    }
    if (start_position >= 0) { }
    else { mess = "start_position out of range"; goto bad; }
    return;
    bad: raise(e_VoiceParameterInvalid, mess);
}

VoiceSpec::operator VoiceImp () const {
    if (!audio) return VoiceImp();
    validate();
    return assume_valid();
}

VoiceImp VoiceSpec::assume_valid () const {
    return VoiceImp{
        .audio = audio,
        .position = geo::round(start_position * chronons_per_second(audio)),
        .speed = geo::round(speed * speed_scale),
        .loop_start = i32(geo::round(loop_start * samples_per_second(audio))),
        .loop_end = std::isnan(loop_end) ? -1
            : loop_end > n_seconds(audio) ? i32(audio->n_samples)
            : i32(geo::round(loop_end * samples_per_second(audio))),
        .volume = volume,
        .fade_volume = volume
    };
}

VoiceImp::operator VoiceSpec () {
    return VoiceSpec{
        .audio = audio,
        .volume = volume,
        .speed = speed / speed_scale,
        .loop_start = loop_start * seconds_per_sample(audio),
        .loop_end = loop_end * seconds_per_sample(audio),
        .start_position = position * seconds_per_chronon(audio)
    };
}

void Mixer::play (u32 channel, const VoiceImp& v) {
    if (channel >= voices.size()) grow_voices(voices, channel);
    voices[channel] = v;
}

u32 Mixer::play_on_free_channel (u32 minimum, const VoiceImp& v) {
    u32 channel;
    for (channel = minimum; channel < voices.size(); channel += 1) {
        if (!voices[channel].audio) goto found;
    }
    grow_voices(voices, channel);
    found:
    voices[channel] = v;
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

void Mixer::set_volume (u32 channel, float v) {
    if (v >= 0 && v <= 16) { }
    else { raise(e_VoiceParameterInvalid, "volume out of range"); }
    if (channel >= voices.size()) return;
    voices[channel].volume = v;
    voices[channel].fade_volume = v; // cancel fade
}

void Mixer::fade (u32 channel, float v, float fade_time) {
    if (v >= 0 && v <= 16) { }
    else { raise(e_VoiceParameterInvalid, "fade_volume out of range"); }
    if (fade_time > 0) { }
    else { raise(e_VoiceParameterInvalid, "fade_time out of range"); }
    if (channel >= voices.size()) return;
    voices[channel].fade_volume = v;
    float diff = v - voices[channel].volume;
    voices[channel].fade_velocity = diff / fade_time;
}

void Mixer::fade_with_speed (u32 channel, float v, float fade_speed) {
    if (v >= 0 && v <= 16) { }
    else { raise(e_VoiceParameterInvalid, "fade_volume out of range"); }
    if (std::isnan(fade_speed)) {
        raise(e_VoiceParameterInvalid, "fade_speed out of range");
    }
    if (channel >= voices.size()) return;
    voices[channel].fade_volume = v;
    float diff = v - voices[channel].volume;
    voices[channel].fade_velocity = std::copysign(v, diff);
}

 // This is awkward because we're combining fixed-point and floating-point math,
 // and also merging the lerp weights with volume scaling.  The compiler isn't
 // allowed to do much floating point optimization, so we have to do it by hand.
 //
 // We're using linear interpolation, which can distort some of the highest
 // frequencies, but given we're working with 40khz+ audio, it shouldn't really
 // be noticable.
[[gnu::hot, gnu::noclone]] static
void mix_voice (
    StereoFloat*__restrict out, u32 out_len, u32 out_rate,
    VoiceImp&__restrict v
) {
    if (!v.audio) return;
    const i16*__restrict in = v.audio->samples;
     // Calculate a bunch of stuff outside of the main loop
    bool stereo = v.audio->n_channels > 1;
     // Don't forget to write this back after looping!
    i64 in_pos = v.position;
    expect(in_pos >= 0);
     // How fast to consume the input compared to the output
    i64 in_speed = (i64(v.speed) << 8) * v.audio->sample_rate / out_rate;
    expect(in_speed >= 0);
     // Figure out how much input we can get before doing something weird.
     // Subtract one from loop_end so we can specially interpolate the last
     // sample.
    i64 in_noncontinuity = v.loop_end >= 0
        ? i64(v.loop_end - 1) << 32
        : n_chronons(v.audio);
     // This is the "one" value for the first lerp weight.
    float w_add = 0x1'0000'0000;
     // This simultaneously:
     //   - descales the lerp weights from 0:32,
     //   - preemptively descales the to-be-multiplied samples from 0:15,
     //   - multiplies the volume in.
    float w_mul = (1.f/0x8000'0000'0000) * v.volume;
#if __SSE4_1__
    auto w_adds = _mm_set_ps(0, 0, -w_add, -w_add);
    auto w_muls = _mm_set_ps(w_mul, w_mul, -w_mul, -w_mul);
    auto input_shuffler = stereo
        ? _mm_set_epi8(-1,-1,-1,-1,-1,-1,-1,-1,7,6,5,4,3,2,1,0)
        : _mm_set_epi8(-1,-1,-1,-1,-1,-1,-1,-1,3,2,3,2,1,0,1,0);
#endif
    expect(out_len > 0);
    for (u32 out_i = 0; out_i < out_len; out_i++) {
        redo:
         // Split position into index and lerper
        u32 in_i = in_pos >> 32;
        in_i <<= stereo;
        float lerper = u32(in_pos);
#if __SSE4_1__
        __m128i s16s;
#else
        i16 s16s [4];
#endif
         // Fetch the input samples, but first check if we need to do something
         // weird.
        if (in_pos < in_noncontinuity) [[likely]] {
#if __SSE4_1__
            s16s = _mm_loadu_si64((u64*)(in + in_i));
            s16s = _mm_shuffle_epi8(s16s, input_shuffler);
#else
            s16s[0] = in[in_i];
            s16s[1] = in[in_i + stereo];
            s16s[2] = in[in_i + stereo + 1];
            s16s[3] = in[in_i + stereo + stereo + 1];
#endif
        }
        else if (v.loop_end < 0) {
             // This voice is done!
            v.audio = null;
            break;
        }
        else if (in_pos >= i64(v.loop_end) << 32) {
             // We're looping!
            in_pos -= i64(v.loop_end - v.loop_start) << 32;
            expect(in_pos < i64(v.loop_end) << 32);
            goto redo;
        }
        else {
             // About to loop!  We have to acquire one sample from before the
             // loop end and one sample from after the loop start.  Note that if
             // in_speed is less than one, we may do this path multiple times
             // before actually looping.
            u32 in_i2 = in_i - ((v.loop_end - v.loop_start - 1) << stereo);
#if __SSE4_1__
            if (stereo) {
                s16s = _mm_loadu_si32((u32*)(in + in_i));
                s16s = _mm_insert_epi32(s16s, *(u32*)(in + in_i2), 1);
            }
            else {
                s16s = _mm_loadu_si16((u16*)(in + in_i));
                s16s = _mm_insert_epi16(s16s, *(u16*)(in + in_i2), 1);
            }
#else
            s16s[0] = in[in_i];
            s16s[1] = in[in_i + stereo];
            s16s[2] = in[in_i2];
            s16s[3] = in[in_i2 + stereo];
#endif
        }
#if __SSE4_1__
         // Convert samples to float
        auto ss = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(s16s));
         // Calculate weights
        auto ws = _mm_set1_ps(lerper);
        ws = _mm_add_ps(ws, w_adds);
        ws = _mm_mul_ps(ws, w_muls);
         // Apply weights
        ss = _mm_mul_ps(ss, ws);
        ss = _mm_add_ps(ss, _mm_shuffle_ps(ss, ss, _MM_SHUFFLE(1,0,3,2)));
         // Accumulate.  I'm not sure if casting floats to doubles incurs a
         // domain-change delay, but it seems less likely than casting to
         // integers.
        auto acc = _mm_castpd_ps(_mm_load_sd((double*)(out + out_i)));
        ss = _mm_add_ps(ss, acc);
        _mm_store_sd((double*)(out + out_i), _mm_castps_pd(ss));
#else
         // Convert samples to float
        float ss [4];
        for (u32 i = 0; i < 4; i++) {
            ss[i] = s16s[i];
        }
         // Calculate weights
        float ws [4];
        ws[0] = ws[1] = (lerper - w_add) * -w_mul;
        ws[2] = ws[3] = lerper * w_mul;
         // Apply weights
        float outs [2];
        for (u32 i = 0; i < 2; i++) {
            outs[i] = ss[i] * ws[i] + ss[i+2] * ws[i+2];
        }
         // Accumulate
        for (u32 i = 0; i < 2; i++) {
            out[out_i][i] += outs[i];
        }
#endif
         // Now bump the fixed-point position along
        in_pos += in_speed;
    }
     // Done looping, so write position back to voice
    v.position = in_pos;
     // Apply fade
    if (!std::isnan(v.fade_velocity)) {
        v.volume += v.fade_velocity * float(out_len) / out_rate;
         // If we pass the goal, stop
        if ((v.fade_volume - v.volume < 0) != (v.fade_velocity < 0)) {
            v.volume = v.fade_volume;
            v.fade_velocity = geo::GNAN;
        }
    }
}

[[gnu::hot, gnu::noclone]]
void Mixer::mix (
    StereoFloat*__restrict out, u32 out_len, u32 out_rate
) {
     // The output buffer is not prezeroed.
    for (auto o = out; o < out + out_len; o++) {
        (*o)[0] = 0;
        (*o)[1] = 0;
    }
    for (auto& v : voices) {
        mix_voice(out, out_len, out_rate, v);
    }
}

} using namespace snd;

#ifndef TAP_DISABLE_TESTS
#include "../ayu/reflection/describe-standard.h"
#include "../ayu/traversal/to-tree.h"
#include "../geo/scalar.h"
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

    VoiceImp v0;
    v0.audio = &in0;
    VoiceImp v1;
    v1.audio = &in1;
    v1.loop_start = 0; // TODO: test non-zero loop_start
    v1.loop_end = 520;

    Mixer mixer;
    mixer.play_on_free_channel(0, v0);
    mixer.play_on_free_channel(0, v1);

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
    about(out[52][0], want(564, 564), "Before loop");
    about(out[52][1], want(564, -564));
     // Not testing the sample right when looping, because it's interpolated
     // specially and I don't want to figure out the math to predict it.
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
