#pragma once

 // A very simple stereo sound mixer.

#include "audio.h"
#include "../geo/values.h"
#include "../uni/arrays.h"

namespace snd {

namespace in {

[[noreturn, gnu::cold]]
void raise_VoiceUnsupported (StaticString);

} // in

struct Voice {
     // None of the below values are preserved if audio is null.
    UniqueAudio* audio = null;
    float volume = 1.f; // Can be above 1 at the risk of clipping
     // 8:24.  Does not keep pitch constant.
    i32 speed = speed_scale;
     // These are in samples.
    i32 loop_start = 0;
    i32 loop_end = -1; // <0 means no loop
     // 31:32 in samples
    i64 position = 0;

     // Some calculation helpers
    static constexpr float speed_scale = 0x100'0000;
    static constexpr double chronons_per_sample = 0x1'0000'0000;
    static constexpr double samples_per_chronon = 1 / chronons_per_sample;
    double samples_per_second () const {
        return audio ? double(audio->sample_rate) : geo::GNAN;
    }
    double seconds_per_sample () const {
        return 1 / samples_per_second();
    }
    double chronons_per_second () const {
        return chronons_per_sample * samples_per_second();
    }
    double seconds_per_chronon () const {
        return seconds_per_sample() * samples_per_chronon;
    }
    i64 n_chronons () const {
        return audio ? i64(audio->n_samples) * chronons_per_sample : 0;
    }
    double n_seconds () const {
        return audio ? i64(audio->n_samples) * seconds_per_sample() : geo::GNAN;
    }

     // floating point accessors and mutators
    float get_volume () const {
        return volume;
    }
    void set_volume (float v) {
        if (v < 0 || v > 16) in::raise_VoiceUnsupported("volume out of range");
        volume = v;
    }
    float get_speed () const {
        return speed * (1.f/0x100'0000);
    }
    void set_speed (float v) {
        if (v < 0 || v > 16) in::raise_VoiceUnsupported("speed out of range");
        speed = v * 0x100'0000;
    }
    double get_position_seconds () const {
        if (!audio) return 0;
        return position * seconds_per_chronon();
    }
    void set_position_seconds (double v) {
        if (!audio) return;
        i64 p = v * chronons_per_second() + 0.5;
        if (p < 0) in::raise_VoiceUnsupported("position out of range");
        i64 limit = n_chronons();
        position = p > limit ? limit : p;
    }
    double get_loop_start_seconds () const {
        if (!audio) return 0;
        return loop_start * seconds_per_sample();
    }
    void set_loop_start_seconds (double v) {
        if (!audio) return;
        i32 p = v * samples_per_second() + 0.5;
        if (p < 0 || p > i32(audio->n_samples)) in::raise_VoiceUnsupported("loop_start out of range");
        loop_start = p;
    }
    double get_loop_end_seconds () const {
        if (!audio) return 0;
        if (loop_end < 0) return geo::GNAN;
        return loop_end * seconds_per_sample();
    }
    void set_loop_end_seconds (double v) {
        if (!audio) return;
        if (std::isnan(v)) { loop_end = -1; return; }
        if (!std::isfinite(v)) { loop_end = audio->n_samples; return; }
        i32 p = v * samples_per_second() + 0.5;
        if (p < 0 || p > i32(audio->n_samples)) in::raise_VoiceUnsupported("loop_end out of range");
        loop_end = p;
    }
};

using StereoFloat = float[2];

 // This does not do any locking or integration with SDL etc.
struct Mixer {
    uni::UniqueArray<Voice> voices;

     // You probably shouldn't be playing this many sounds at once.
    static constexpr u32 highest_channel = 127;

     // Play voice on specific channel.  If the channel is already playing a
     // voice, will stop the old voice.  The voice data will be copied.
    void play (const Voice&, u32 channel);
     // Play voice on an unused channel, which must be at least minimum.  Returns
     // the actual channel picked.
    u32 play_on_free_channel (const Voice&, u32 minimum = 0);

    void stop (u32 channel);
    void stop_all ();

     // Run the mixer.
     //   - out: pointer to out_len pairs of floats.  Should not be prezeroed.
     //   - out_len: length (in pairs of floats) of out.
     //   - out_rate: desired output sample_rate (e.g. 48000).
    void mix (StereoFloat* out, u32 out_len, u32 out_rate);
};

static constexpr uni::ErrorCode e_VoiceUnsupported = "snd::e_VoiceUnsupported";

} // snd
