#pragma once

 // Modern C++'s chrono system is...complicated.  Let's just work with
 // double-precision times in seconds.

#include <chrono>

namespace uni {

inline double system_clock () {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

inline double now () { return system_clock(); }

inline double steady_clock () {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

} // namespace uni
