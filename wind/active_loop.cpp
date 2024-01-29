#include "active_loop.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_timer.h>

#include "../ayu/reflection/describe.h"

namespace wind {

static void default_on_event (ActiveLoop& self, SDL_Event* event) {
    switch (event->type) {
        case SDL_QUIT: {
            self.stop();
            break;
        }
        case SDL_KEYDOWN: {
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                self.stop();
            }
            break;
        }
    }
}

void ActiveLoop::start () {
    double lag = 0;
    uint32 last_ticks = SDL_GetTicks();
    for (;;) {
         // Handle events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            bool handled = on_event && on_event(&event);
            if (!handled) default_on_event(*this, &event);
        }
        if (stop_requested) break;
         // Run step
        if (on_step) on_step();
         // Calculate timing
        lag -= 1/fps;
        if (lag > max_lag_tolerance/fps) {
             // Really bad lag!  Is step() taking too long?
            lag = 1/fps;
        }
        if (lag > (1 + min_lag_tolerance)/fps) {
             // Drop frame
        }
        else {
             // Tolerate tiny amount of lag
            if (lag > 1/fps) lag = 1/fps;
             // Render
            if (on_draw) on_draw();
        }
         // Delay if we need to.
         // TODO: Is this the best place to put this?
        uint32 new_ticks = SDL_GetTicks();
        lag += (new_ticks - last_ticks) / 1000.0;
        last_ticks = new_ticks;
        if (lag < 0) {
            SDL_Delay(-lag * 1000);
        }
    }
}

void ActiveLoop::stop () noexcept {
    stop_requested = true;
}

} using namespace wind;

AYU_DESCRIBE(wind::ActiveLoop,
    attrs(
        attr("fps", &ActiveLoop::fps, optional),
        attr("min_lag_tolerance", &ActiveLoop::min_lag_tolerance, optional),
        attr("max_lag_tolerance", &ActiveLoop::max_lag_tolerance, optional)
    )
)
