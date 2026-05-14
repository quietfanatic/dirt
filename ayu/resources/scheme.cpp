#include "scheme.h"

#include "resource.private.h"

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

void ResourceScheme::activate (const SharedString& name) noexcept {
    require(iri::scheme_canonical(name));
    auto hash = uni::hash(name);
    auto& schemes = g_universe->schemes;
    for (auto& entry : schemes) {
        if (entry.hash == hash && entry.name == name) {
            require(false);
        }
    }
    schemes.emplace_back(hash, name, this);
}
void ResourceScheme::deactivate () noexcept {
    auto& schemes = g_universe->schemes;
    u32 i = 0;
    while (i < schemes.size()) {
        if (schemes[i].scheme == this) {
            schemes.erase(i);
        }
        else i++;
    }
}

 // The default scheme.  Don't activate this because it is only used when there
 // are no active schemes.
constinit auto default_scheme = FolderResourceScheme("file:/"_iri);

ResourceScheme* get_scheme (const IRI& name) {
    Str scheme = name.scheme();
    if (auto& schemes = g_universe->schemes) {
        usize hash = uni::hash(scheme);
        for (auto& entry : schemes) {
            if (entry.hash == hash && entry.name == scheme) {
                return entry.scheme;
            }
        }
    }
    else {
        if (scheme == "file") return &default_scheme;
    }
    return null;
}

ResourceScheme* require_scheme (const IRI& name) {
    auto r = get_scheme(name);
    if (!r) raise(e_ResourceSchemeNotFound, "This resource's scheme has no handler");
    return r;
}

} // ayu
