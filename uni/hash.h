#pragma once

#include <array>
#include <bit>
#include "common.h"

namespace uni {

///// HasBytes<T>::for_bytes
// A specializable template for anything that can be viewed as a series of bytes

template <class T>
struct HasBytes;

template <class T> requires (std::is_scalar_v<T>)
struct HasBytes<T> {
    template <class F>
    static constexpr void for_bytes (const T& v, const F& f) {
        auto bytes = std::bit_cast<std::array<char, sizeof(T)>>(v);
        for (usize i = 0; i < sizeof(T); ++i) f(bytes[i]);
    }
};

template <class T, usize n>
struct HasBytes<T[n]> {
    template <class F>
    static constexpr void for_bytes (const T(& v )[n], const F& f) {
        for (usize i = 0; i < n; ++i) {
             // Special treatment for character arrays: Stop processing when we
             // reach a NUL
            if constexpr (std::is_same_v<std::remove_cvref_t<T>, char>
                       || std::is_same_v<std::remove_cvref_t<T>, wchar_t>
                       || std::is_same_v<std::remove_cvref_t<T>, char8_t>
                       || std::is_same_v<std::remove_cvref_t<T>, char16_t>
                       || std::is_same_v<std::remove_cvref_t<T>, char32_t>
            ) {
                if (!v[i]) return;
            }
            HasBytes<std::remove_cvref_t<T>>::for_bytes(v[i], f);
        }
    }
};

template <class T> requires (
    requires (const T v) { v.begin() != v.end(); }
)
struct HasBytes<T> {
    template <class F>
    static constexpr void for_bytes (const T& v, const F& f) {
        for (const auto& e : v) {
            HasBytes<std::remove_cvref_t<decltype(e)>>::for_bytes(e, f);
        };
    }
};

///// HASHING
// Using FNV-1a.  This isn't the fastest hash on modern CPUs, because it can
// only do one byte at a time, but it's very small.

template <class T>
constexpr u64 hash64 (const T& v) {
    u64 h = 0xcbf29ce484222325;
    HasBytes<T>::for_bytes(v, [&h](char c){
        h = (h ^ u8(c)) * 0x100000001b3;
    });
    return h;
}

template <class T>
constexpr u32 hash32 (const T& v) {
    u32 h = 0x811c9dc5;
    HasBytes<T>::for_bytes(v, [&h](char c){
        h = (h ^ u8(c)) * 0x1000193;
    });
    return h;
}

template <class T>
constexpr usize hash (const T& v) {
    if constexpr (sizeof(usize) == 8) return hash64(v);
    else if constexpr (sizeof(usize) == 4) return hash32(v);
    else static_assert(sizeof(usize) == 8 || sizeof(usize) == 4);
}

 // Returns a hash with only the given number of bits.  Any higher bits are 0.
template <class T>
constexpr T hash_fold (T h, u8 bits) {
     // If the desired number of bits is smaller than half of usize, just throw
     // away the middle bits.  This still throws away fewer bits than % does.
    T low = h & ((1 << bits) - 1);
    T high = h >> (sizeof(T) * 8 - bits);
    return low ^ high;
}

 // Combine two hashes into one
template <class T>
constexpr T hash_combine (T a, T b) {
    return a * 3 + b;
}

} // namespace uni
