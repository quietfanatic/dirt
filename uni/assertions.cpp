#include "assertions.h"

#include <cstdlib>
#include "strings.h"
#include "io.h"

namespace uni {

[[gnu::cold]]
void abort_assertion_failed (std::source_location loc) noexcept {
    warn_utf8(cat(
        "ERROR: Assertion failed at ", loc.file_name(), ':',
        loc.line(), "\n       in ", loc.function_name(), "\n"
    ));
    std::abort();
}

} // uni
