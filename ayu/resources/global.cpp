#include "global.h"

#include "universe.private.h"

namespace ayu {
using namespace in;

void global (const AnyPtr& ref) {
    expect(ref);
#ifndef NDEBUG
    for (auto& g : universe().globals) {
        expect(g != ref);
    }
#endif
    universe().globals.push_back(ref);
}

void unregister_global (const AnyPtr& ref) {
    auto& gs = universe().globals;
    for (auto& g : gs) {
        if (g == ref) {
            gs.erase(&g);
            return;
        }
    }
}

} // ayu
