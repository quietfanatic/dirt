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

void ResourceScheme::activate () {
    require(iri::scheme_canonical(name));
    auto hash = uni::hash(name);
    auto& schemes = g_universe->schemes;
    for (auto [h, s] : schemes) {
        if (h == hash && s->name == name) {
            require(false);
        }
    }
    schemes.emplace_back(hash, this);
}
void ResourceScheme::deactivate () noexcept {
    auto& schemes = g_universe->schemes;
    for (auto& p : schemes) {
        if (p.value == this) {
            schemes.erase(&p);
            return;
        }
    }
}

 // The default scheme.  Don't auto_activate because this is only used when
 // there are no active schemes.
constinit auto default_scheme = FolderResourceScheme("file", "file:/"_iri, false);

ResourceScheme* get_scheme (const IRI& name) {
    Str scheme = name.scheme();
    if (auto schemes = g_universe->schemes) {
        usize hash = uni::hash(scheme);
        for (auto [h, s] : schemes) {
            if (h == hash && s->name == scheme) {
                return s;
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
    if (!r) raise(e_ResourceSchemeNotFound, name.spec());
    return r;
}

} // ayu
