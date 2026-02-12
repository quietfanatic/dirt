 // This includes implementations for multiple headers
#include "resource-scheme.h"
#include "universe.private.h"

namespace ayu {

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

namespace in {
using namespace iri::literals;

ResourceRef Universe::get_resource (const IRI& name) {
    Str spec = expect(name.spec());
    expect(spec.begin() < spec.end());
    usize h = uni::hash(spec);

    for (auto& r : resources) {
        if (r.hash == h && r.value->name().spec_ == spec) {
            return r.value;
        }
    }
    auto data = new ResourceData(name);
    return resources.emplace_back(h, data).value;
}

void Universe::delete_resource (ResourceRef r) {
    for (auto& e : resources) {
        if (e.value == r) {
            resources.erase(&e);
            delete r.data;
            return;
        }
    }
    require(false);
}

 // The default scheme.  Don't auto_activate because this is only used when
 // there are no active schemes.
constinit auto default_scheme = FolderResourceScheme("file", "file:/"_iri, false);

void Universe::register_scheme (ResourceScheme* scheme) {
    usize h = hash(scheme->name);
    for (auto& s : schemes) {
        if (s.hash == h && s.value->name == scheme->name) {
            require(false);
        }
    }
    schemes.emplace_back(h, scheme);
}

void Universe::unregister_scheme (ResourceScheme* scheme) {
    usize h = hash(scheme->name);
    for (auto& s : schemes) {
        if (s.hash == h && s.value->name == scheme->name) {
            schemes.erase(&s);
            return;
        }
    }
}

ResourceScheme* Universe::require_scheme (const IRI& name) {
    Str scheme = name.scheme();
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

} using namespace in;

void ResourceScheme::activate () {
    require(iri::scheme_canonical(name));
    g_universe->register_scheme(this);
}
void ResourceScheme::deactivate () noexcept {
    g_universe->unregister_scheme(this);
}

} // ayu
