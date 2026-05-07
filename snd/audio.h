#pragma once

#include "../ayu/resources/extension.h"
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

    double n_seconds () const { return double(n_samples) / sample_rate; }

    constexpr UniqueAudio () { }

    UniqueAudio (u32 c, u32 s, u32 r) :
        n_channels(c), n_samples(s), sample_rate(r),
         // A little extra room makes vectorizing easier
        samples(new i16[c * s + 4])
    {
         // Go ahead and zero out the extra space to prevent popping when
         // reaching the end of the audio.
        for (usize i = 0; i < 4; i++) {
            samples[c * s + i] = 0;
        }
    }

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

UniqueAudio audio_from_blob (Slice<u8> content, Str filename = "");
UniqueAudio audio_from_file (SharedString filename);
UniqueAudio audio_from_blob_qoa (Slice<u8> blob, Str filename = "");
UniqueAudio audio_from_blob_wav (Slice<u8> blob, Str filename = "");
UniqueArray<u8> audio_to_blob_qoa (const UniqueAudio&, Str filename = "");
void audio_to_file_qoa (const UniqueAudio&, SharedString filename);

constexpr ErrorCode e_LoadAudioFailed = "snd::e_LoadAudioFailed";

 // An extension to use audio files as AYU resources.  Unlike images, audio
 // needs to remain in CPU memory, so there's little reason to have a lazy
 // loading system.
struct AudioExtensionQOA : ayu::ResourceExtension {
    bool accepts_type (ayu::Type) override; // Only accepts UniqueAudio
    void from_blob (ayu::AnyVal&, Slice<u8>, ayu::ResourceRef, ayu::ResourceScheme*);
    UniqueArray<u8> to_blob (const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions);
    using ayu::ResourceExtension::ResourceExtension;
};

struct AudioExtensionWAV : ayu::ResourceExtension {
    bool accepts_type (ayu::Type) override; // Only accepts UniqueAudio
    void from_blob (ayu::AnyVal&, Slice<u8>, ayu::ResourceRef, ayu::ResourceScheme*);
    UniqueArray<u8> to_blob (const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions);
    using ayu::ResourceExtension::ResourceExtension;
};

} // snd

namespace snd::in {

[[noreturn, gnu::cold]]
void raise_LoadAudioFailed(Str filename, Str mess);

} // snd::in

