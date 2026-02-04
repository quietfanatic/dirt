#pragma once

#include "../uni/common.h"
#include "../uni/strings.h"
#include "../uni/errors.h"

namespace snd {
using namespace uni;

struct UniqueAudio {
    u32 n_channels = 0;
    u32 n_samples = 0; // per channel
    u32 sample_rate = 0; // hz
    i16* samples = null;

    usize size () const { return n_channels * n_samples; }
    i16* data () { return samples; }
    const i16* data () const { return samples; }

    constexpr UniqueAudio () { }

    UniqueAudio (u32 c, u32 s, u32 r) :
        n_channels(c), n_samples(s), sample_rate(r),
         // Temporary until we make the qoa decoder stop properly
        samples(new i16[c * s])
    { }

    constexpr
    ~UniqueAudio () {
        if (samples) delete[] samples;
    }

    constexpr
    UniqueAudio (UniqueAudio&& o) :
        n_channels(o.n_channels),
        n_samples(o.n_samples),
        sample_rate(o.sample_rate),
        samples(o.samples)
    { o.samples = null; }

    constexpr
    UniqueAudio& operator= (UniqueAudio&& o) {
        if (samples) [[unlikely]] delete[] samples;
        n_channels = o.n_channels;
        n_samples = o.n_samples;
        sample_rate = o.sample_rate;
        samples = o.samples;
        o.samples = null;
        return *this;
    }

    constexpr explicit operator bool () const { return samples; }
};

UniqueAudio audio_from_array (Slice<u8> content, Str filename = "");
UniqueAudio audio_from_file (AnyString filename);
UniqueAudio audio_from_array_qoa (Slice<u8> contents, Str filename);
UniqueAudio audio_from_array_wav (Slice<u8> contents, Str filename);
UniqueArray<u8> audio_to_array_qoa (const UniqueAudio&);
void audio_to_file_qoa (const UniqueAudio&, AnyString filename);

constexpr ErrorCode e_LoadAudioFailed = "snd::e_LoadAudioFailed";

} // snd

namespace snd::in {

[[noreturn, gnu::cold]]
void raise_LoadAudioFailed(Str filename, Str mess);

} // snd::in

