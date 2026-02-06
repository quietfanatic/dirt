#pragma once

 // This is a wrapper around Mixer that uses SDL locking.

#include <SDL2/SDL_audio.h>
#include "mixer.h"

namespace snd {

struct MixerSDL {
    Mixer core;
    u32 sdl_device = 0;
    SDL_AudioSpec sdl_audio_spec;

     // Start the SDL audio device, taking input from the core mixer.
    void start_output (u32 buffer = 512, u32 rate = 48000);
     // Shut down the SDL audio device.  This may be slow, so don't do it just
     // to pause the audio.
    void stop_output ();

    void play (const Voice& v, u32 channel) {
        SDL_LockAudioDevice(sdl_device);
        core.play(v, channel);
        SDL_UnlockAudioDevice(sdl_device);
    }
    void play_on_free_channel (const Voice& v, u32 minimum = 0) {
        SDL_LockAudioDevice(sdl_device);
        core.play_on_free_channel(v, minimum);
        SDL_UnlockAudioDevice(sdl_device);
    }
    void stop (u32 channel) {
        SDL_LockAudioDevice(sdl_device);
        core.stop(channel);
        SDL_UnlockAudioDevice(sdl_device);
    }
    void stop_all () {
        SDL_LockAudioDevice(sdl_device);
        core.stop_all();
        SDL_UnlockAudioDevice(sdl_device);
    }
};

} // snd
