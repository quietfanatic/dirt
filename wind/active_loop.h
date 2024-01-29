#pragma once

#include <functional>
#include "../geo/vec.h"
#include "../uni/common.h"

union SDL_Event;

namespace wind {
using namespace uni;

 // An active loop using SDL.  The default on_step calls poll_events.
 // WARNING: This hasn't been used in a while so it may be a bit stale.
 // TODO: Replace std::function with uni::UniqueFunction when we start using
 // this.
struct ActiveLoop {
     // Desired framerate.
    double fps = 60;
     // If lag is less than this amount in frames, slow down instead of dropping
     //  frames.  This will allow playing on monitors vsynced to 59.9hz or
     //  something like that without dropping any frames.
    double min_lag_tolerance = 0.005;  // 60 -> 59.7
     // If lag is more than this amount in frames, slow down instead of dropping
     //  frames.  The game will be barely playable, but it's better than being
     //  frozen.
    double max_lag_tolerance = 3.0;

     // Will be called before on_step for each currently queued SDL event.  If
     // the function returns true, the event is considered handled, otherwise a
     // default handler will run (which just stops the loop on SDL_Quit and
     // pressing Escape).
    std::function<bool(SDL_Event*)> on_event = null;
     // Will be called at the desired fps, unless slowdown happens.
    std::function<void()> on_step = null;
     // Will be called at the desired fps, unless frameskip or slowdown happens.
    std::function<void()> on_draw = null;

     // stop() has been called.
    bool stop_requested = false;

     // Loops over update and draw until stop is called
    void start ();
     // Makes start() return.
    void stop () noexcept;
};

} // namespace wind
