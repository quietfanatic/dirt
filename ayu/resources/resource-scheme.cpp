#include "resource-scheme.h"

#include "universe.private.h"

namespace ayu {
using namespace in;
using namespace iri::literals;

[[noreturn, gnu::cold]]
void raise_ResourceNameRejected () {
    raise(e_ResourceNameRejected, "Resource name is not valid for this scheme");
}

[[noreturn, gnu::cold]]
void raise_ResourceTypeRejected (Type type) {
    Error e;
    e.code = e_ResourceTypeRejected;
    e.details = "Resource type is not valid for this scheme";
    e.add_tag("ayu::Type", type.name());
    throw e;
}

[[noreturn, gnu::cold]]
void raise_ResourceNoFilepath () {
    raise(e_ResourceNoFilepath, "Resource has no associated filepath.");
}

void ResourceScheme::activate () {
    require(iri::scheme_canonical(name));
    auto& schemes = g_universe->schemes;
    usize h = hash(name);
    for (auto& s : schemes) {
        if (s.hash == h && s.value->name == name) {
            require(false);
        }
    }
    schemes.emplace_back(h, this);
}
void ResourceScheme::deactivate () noexcept {
    auto& schemes = g_universe->schemes;
    usize h = hash(name);
    for (auto& s : schemes) {
        if (s.value == this) {
            schemes.erase(&s);
            return;
        }
    }
}

 // The default scheme.  Don't auto_activate because this is only used when
 // there are no active schemes.
constinit auto default_scheme = FolderResourceScheme("file", "file:/"_iri, false);

ResourceScheme* require_scheme (const IRI& name) {
    Str scheme = name.scheme();
    auto schemes = g_universe->schemes;
    if (schemes) {
        usize h = hash(scheme);
        for (auto& s : schemes) {
            if (s.hash == h && s.value->name == scheme) {
                return s.value;
            }
        }
    }
    else {
        if (scheme == "file") return &default_scheme;
    }
    raise(e_ResourceSchemeNotFound, name.spec());
}

} // ayu
