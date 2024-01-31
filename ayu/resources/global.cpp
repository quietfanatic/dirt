#include "global.h"

#include "universe.private.h"

namespace ayu {
using namespace in;

void global (const Pointer& ref) {
    expect(ref);
    universe().globals.push_back(ref);
}

void unregister_global (const Pointer& ref) {
    auto& gs = universe().globals;
    for (auto& g : gs) {
        if (g == ref) {
            gs.erase(&g);
            return;
        }
    }
}

} // ayu
