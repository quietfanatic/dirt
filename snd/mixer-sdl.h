#pragma once

 // This is a wrapper around Mixer that outputs to an SDL audio device.

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

    void play (u32 channel, const VoiceImp& v) {
        lock();
        core.play(channel, v);
        unlock();
    }
    void play_on_free_channel (u32 minimum, const VoiceImp& v) {
        lock();
        core.play_on_free_channel(minimum, v);
        unlock();
    }
    void stop (u32 channel) {
        lock();
        core.stop(channel);
        unlock();
    }
    void stop_all () {
        lock();
        core.stop_all();
        unlock();
    }
    void set_volume (u32 channel, float v) {
        lock();
        core.set_volume(channel, v);
        unlock();
    }
    void fade (u32 channel, float v, float fade_time) {
        lock();
        core.fade(channel, v, fade_time);
        unlock();
    }
    void fade_with_speed (u32 channel, float v, float fade_speed) {
        lock();
        core.fade_with_speed(channel, v, fade_speed);
        unlock();
    }

     // Manually lock.  Don't forget to unlock!
    void lock () { SDL_LockAudioDevice(sdl_device); }
    void unlock () { SDL_UnlockAudioDevice(sdl_device); }
};

} // snd
