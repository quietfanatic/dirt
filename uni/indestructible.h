#pragma once
#include <utility>

namespace uni {

template <class T>
union Indestructible {
    T v;
    template <class... Args>
    constexpr Indestructible (Args&&... args) : v(std::forward<Args>(args)...) { }
    constexpr T& operator* () { return v; }
    constexpr T* operator-> () { return &v; }
    constexpr ~Indestructible () { }
};

} // uni
