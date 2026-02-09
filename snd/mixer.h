#pragma once

 // A very simple stereo sound mixer.

#include "audio.h"
#include "../geo/values.h"
#include "../uni/arrays.h"

namespace snd {

struct VoiceImp;

struct VoiceSpec {
    UniqueAudio* audio = null;
    float volume = 1.f; // Can be above 1 at the risk of clipping
    float speed = 1.f; // Does not keep pitch constant
    double loop_start = 0; // In seconds, will be snapped to input samples
    double loop_end = geo::GNAN; // NAN means no loop, INF means end of audio
    double start_position = 0;

     // Throws e_VoiceParameterInvalid if:
     //   - audio is null or isn't mono or stereo
     //   - volume isn't between 0 and 16
     //   - speed isn't between 0 and 16
     //   - loop_start is outside audio
     //   - loop_end is negative or before loop_start
     //   - start_position is outside audio
    void validate () const;

     // Calls validate() before converting.  We're using out-conversions instead
     // of in-conversions because defining any constructors whatsoever prevents
     // aggregate initialization.
    operator VoiceImp () const;

     // Does not validate!
    VoiceImp assume_valid () const;
};

struct VoiceImp {
    UniqueAudio* audio = null;
     // This is in chronons (31:32 in input samples)
    i64 position = 0;
     // 7:24.  Does not keep pitch constant.
    i32 speed = 0x100'0000;
     // These are in input samples.
    i32 loop_start = 0;
    i32 loop_end = -1; // <0 means no loop
    float volume = 1.f; // This is still floating point though
    float fade_volume = 1.f; // No fade if same as volume
    float fade_velocity = geo::GNAN; // Signed, in volume units per second

     // Does not validate (not much need to).  Does not preserve fade state.
    operator VoiceSpec ();
};

 // The output samples of the mixer
using StereoFloat = float[2];

 // This does not do any locking or integration with SDL etc.
struct Mixer {
    uni::UniqueArray<VoiceImp> voices;

     // You probably shouldn't be playing this many sounds at once.
    static constexpr u32 highest_channel = 127;

     // Play voice on specific channel.  If the channel is already playing a
     // voice, will stop the old voice.  The voice data will be copied.
    void play (u32 channel, const VoiceImp&);
     // Play voice on an unused channel, which must be at least minimum.  Returns
     // the actual channel picked.
    u32 play_on_free_channel (u32 minimum, const VoiceImp&);

    void stop (u32 channel);
    void stop_all ();

     // Set the volume of a channel.  Replaces the original volume instead of
     // multiplying with it.  Cancels any fades.
    void set_volume (u32 channel, float volume);

     // Fade to the given volume over the given amount of time (in seconds).  If
     // already fading, stops the old fade and uses the partially-faded current
     // volume as the new start volume.
    void fade (u32 channel, float fade_volume, float fade_time);
     // Fade at the given speed (volume units per second, e.g. fade_speed=0.2
     // fades from 0.6 to 0 in 3 seconds).
    void fade_with_speed (u32 channel, float fade_volume, float fade_speed);

     // Run the mixer.
     //   - out: pointer to out_len pairs of floats.  Should not be prezeroed.
     //   - out_len: length (in pairs of floats) of out.
     //   - out_rate: desired output sample_rate (e.g. 48000).
    void mix (StereoFloat* out, u32 out_len, u32 out_rate);
};

 // Some math helpers
static constexpr float speed_scale = 0x100'0000;
static constexpr i64 chronons_per_sample = 0x1'0000'0000;
static constexpr double samples_per_chronon = 1 / chronons_per_sample;
static inline double samples_per_second (const UniqueAudio* audio) {
    return audio ? double(audio->sample_rate) : geo::GNAN;
}
static inline double seconds_per_sample (const UniqueAudio* audio) {
    return 1 / samples_per_second(audio);
}
static inline double n_seconds (const UniqueAudio* audio) {
    return audio ? audio->n_seconds() : geo::GNAN;
}
static inline double chronons_per_second (const UniqueAudio* audio) {
    return chronons_per_sample * samples_per_second(audio);
}
static inline double seconds_per_chronon (const UniqueAudio* audio) {
    return seconds_per_sample(audio) * samples_per_chronon;
}
static inline i64 n_chronons (const UniqueAudio* audio) {
    return audio ? i64(audio->n_samples) * chronons_per_sample : 0;
}

static constexpr uni::ErrorCode e_VoiceParameterInvalid = "snd::e_VoiceParameterInvalid";

} // snd
