#pragma once

#include "../uni/common.h"
#include "../uni/unique-function.h"

union SDL_Event;

namespace wind {
using namespace uni;

 // A passive event loop using SDL
struct PassiveLoop {
     // Will be called whenever there is an SDL event.  If this is null, the
     // default behavior is to listen for SDL_QUIT or the escape key and stop.
    UniqueFunction<void(SDL_Event*)> on_event = null;
     // Will be called when the event queue runs out.  If you don't need to do
     // anything, return false, and the loop will go to sleep until a new event
     // arrives.
    UniqueFunction<bool()> on_idle = null;

     // stop() has been called.
    bool stop_requested = false;

     // Loops over update and draw until stop is called
    void start ();
     // Makes start() return.
    void stop () noexcept;
};

} // namespace wind
