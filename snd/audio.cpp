#include "audio.h"

#include "../ayu/reflection/describe-standard.h"
#include "../ayu/resources/resource.h"
#include "../uni/common.h"
#include "../uni/endian.h"
#include "../uni/io.h"

namespace snd {
using namespace uni;

void in::raise_LoadAudioFailed (StaticString mess) {
    raise(e_LoadAudioFailed, mess);
}

UniqueAudio audio_from_blob (Slice<u8> blob) {
    if (blob.size() < 4) in::raise_LoadAudioFailed("File is too short");
    u32 magic = read_u32le(blob.begin());
    if (magic == read_u32le("qoaf")) {
        return audio_from_blob_qoa(blob);
    }
    else if (magic == read_u32le("RIFF")) {
        return audio_from_blob_wav(blob);
    }
    else in::raise_LoadAudioFailed("Unknown magic number");
}

UniqueAudio audio_from_file (const char* path) {
    auto blob = blob_from_file(path);
    try { return audio_from_blob(blob); }
    catch (Error& e) { e.rethrow_with_tag("uni::FilePath", path); }
}
UniqueAudio audio_from_file (Str path) {
    return with_c_str(path, [](auto cs){ return audio_from_file(cs); });
}

void audio_to_file_qoa (const char* path, const UniqueAudio& au) try {
    auto blob = audio_to_blob_qoa(au);
    blob_to_file(path, blob);
} catch (Error& e) { e.rethrow_with_tag("uni::FilePath", path); }

bool AudioExtensionQOA::accepts_type (ayu::Type type) {
    return type == ayu::Type::of<UniqueAudio>();
}
void AudioExtensionQOA::from_blob (
    ayu::AnyVal& value, Slice<u8> blob,
    ayu::ResourceRef, ayu::ResourceScheme* scheme
) {
    scheme->validate_type(ayu::Type::of<UniqueAudio>());
    UniqueAudio au = audio_from_blob_qoa(blob);
    assume(!value);
    value = ayu::AnyVal::make<UniqueAudio>(move(au));
}

UniqueArray<u8> AudioExtensionQOA::to_blob (
    const ayu::AnyVal& value, ayu::ResourceRef, ayu::PrintOptions
) {
    assume(value.type == ayu::Type::of<UniqueAudio>());
    return audio_to_blob_qoa(value.as<UniqueAudio>());
}

bool AudioExtensionWAV::accepts_type (ayu::Type type) {
    return type == ayu::Type::of<UniqueAudio>();
}
void AudioExtensionWAV::from_blob (
    ayu::AnyVal& value, Slice<u8> blob,
    ayu::ResourceRef, ayu::ResourceScheme* scheme
) {
    scheme->validate_type(ayu::Type::of<UniqueAudio>());
    UniqueAudio au = audio_from_blob_wav(blob);
    assume(!value);
    value = ayu::AnyVal::make<UniqueAudio>(move(au));
}

UniqueArray<u8> AudioExtensionWAV::to_blob (
    const ayu::AnyVal&, ayu::ResourceRef, ayu::PrintOptions
) {
    raise(e_General, "Writing WAV files is NYI");
}

} using namespace snd;

 // TODO
AYU_DESCRIBE(snd::UniqueAudio)

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
    auto qoa_raw = blob_from_file(cat(dir, "ui_wood_error.qoa"));
    {
        auto qoa = audio_from_blob(qoa_raw);
        auto qoa_wav = audio_from_file(cat(dir, "ui_wood_error.qoa.wav"));
        if (!is(qoa, qoa_wav, "Decode qoa")) {
            auto wrong = Slice<u8>((u8*)qoa.samples, qoa.size() * 2);
            blob_to_file(cat(dir, "ui_wood_error.s16le.test"), wrong);
        }
    }
    {
        auto wav = audio_from_file(cat(dir, "ui_wood_error.wav"));
        auto encoded = audio_to_blob_qoa(wav);
        if (!is(encoded, qoa_raw, "Encode qoa")) {
            blob_to_file(cat(dir, "ui_wood_error.qoa.test"), encoded);
        }
    }
    done_testing();
});

#endif

