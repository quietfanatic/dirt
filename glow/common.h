#pragma once

#include <source_location>
#include "../uni/arrays.h"
#include "../uni/common.h"
#include "../uni/errors.h"
#include "../uni/strings.h"

namespace iri { struct IRI; }

namespace glow {
using namespace uni;
using iri::IRI;

void init () noexcept;

[[noreturn]]
void requirement_failed_sdl (
    std::source_location loc = std::source_location::current()
) noexcept;

template <class T>
[[gnu::always_inline]]
static constexpr T&& require_sdl (
    T&& v, std::source_location loc = std::source_location::current()
) {
    if (!v) [[unlikely]] requirement_failed_sdl(loc);
    return std::forward<T>(v);
}

} // namespace glow
