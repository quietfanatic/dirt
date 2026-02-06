#include "audio.h"

#include "../uni/common.h"
#include "../uni/endian.h"
#include "../uni/io.h"

namespace snd {
using namespace uni;

void in::raise_LoadAudioFailed (Str filename, Str mess) {
    if (!filename) filename = "string";
    raise(e_LoadAudioFailed, cat(
        "Failed to load audio from ", filename, ": ", mess
    ));
}

UniqueAudio audio_from_array (Slice<u8> contents, Str filename) {
    if (contents.size() < 4) in::raise_LoadAudioFailed(filename, "File is too short");
    u32 magic = read_u32le(contents.begin());
    if (magic == read_u32le("qoaf")) {
        return audio_from_array_qoa(contents, filename);
    }
    else if (magic == read_u32le("RIFF")) {
        return audio_from_array_wav(contents, filename);
    }
    else in::raise_LoadAudioFailed(filename, "Unknown magic number");
}

UniqueAudio audio_from_file (AnyString filename) {
    auto content = array_from_file(filename);
    return audio_from_array(content, filename);
}

void audio_to_file_qoa (const UniqueAudio& au, AnyString filename) {
    auto contents = audio_to_array_qoa(au);
    array_to_file(contents, move(filename));
}

} using namespace snd;

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"
#include "../whereami/whereami.h"
#include "../uni/io.h"

namespace snd {
    static
    bool operator== (const UniqueAudio& a, const UniqueAudio& b) {
        return a.n_channels == b.n_channels
            && a.n_samples == b.n_samples
            && a.sample_rate == b.sample_rate
            && std::memcmp(a.samples, b.samples, a.n_channels * a.n_samples) == 0;
    }
}

static tap::TestSet tests ("dirt/snd/audio", []{
    using namespace tap;
    int len = wai_getExecutablePath(null, 0, null);
    auto dir = UniqueString(Uninitialized(len));
    wai_getExecutablePath(dir.data(), len, null);
    while (dir.back() != '/') dir.pop_back();
    encat(dir, "res/dirt/snd/test/");
    auto qoa_raw = array_from_file(cat(dir, "ui_wood_error.qoa"));
    {
        auto qoa = audio_from_array(qoa_raw);
        auto qoa_wav = audio_from_file(cat(dir, "ui_wood_error.qoa.wav"));
        if (!is(qoa, qoa_wav, "Decode qoa")) {
            auto wrong = Slice<u8>((u8*)qoa.samples, qoa.size() * 2);
            array_to_file(wrong, cat(dir, "ui_wood_error.s16le.test"));
        }
    }
    {
        auto wav = audio_from_file(cat(dir, "ui_wood_error.wav"));
        auto encoded = audio_to_array_qoa(wav);
        if (!is(encoded, qoa_raw, "Encode qoa")) {
            array_to_file(encoded, cat(dir, "ui_wood_error.qoa.test"));
        }
    }
    done_testing();
});

#endif

