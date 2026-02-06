#include "mixer-sdl.h"

#include <SDL2/SDL.h>
#include "../glow/common.h"

namespace snd {

namespace in {

void SDLCALL mix_sdl (void* userdata, u8* out, int out_size) {
    auto& self = *(MixerSDL*)userdata;
    expect(out_size % sizeof(StereoFloat) == 0);
    self.core.mix(
        (StereoFloat*)out,
        out_size / sizeof(StereoFloat),
        self.sdl_audio_spec.freq
    );
}

} // in

void MixerSDL::start_output (u32 buffer, u32 rate) {
    if (sdl_device) return;
    SDL_AudioSpec desired = {};
    desired.freq = rate;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;
    desired.samples = buffer;
    desired.callback = &in::mix_sdl;
    desired.userdata = this;
    glow::require_sdl(!SDL_InitSubSystem(SDL_INIT_AUDIO));
    sdl_device = glow::require_sdl(SDL_OpenAudioDevice(
        null, false, &desired, &sdl_audio_spec,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE
    ));
    SDL_PauseAudioDevice(sdl_device, 0);
}

void MixerSDL::stop_output () {
    SDL_CloseAudioDevice(sdl_device);
    sdl_device = 0;
}

} // snd
