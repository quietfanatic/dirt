#include "extension.h"

#include "resource.private.h"
#include "../data/parse.h"
#include "../traversal/compound.h"
#include "../traversal/from-tree.h"
#include "../traversal/to-tree.h"

namespace ayu {
using namespace in;

void raise_ResourceTypeRejectedByExtension (Type type) {
    Error e;
    e.code = e_ResourceTypeRejected;
    e.details = "Resource type is not valid for this extension";
    e.add_tag("ayu::Type", type.name());
    throw e;
}

void ResourceExtension::from_blob (
    AnyVal& value, Slice<u8> blob, ResourceRef res, ResourceScheme* scheme
) {
    Tree tree = tree_from_string(Str(blob));
    auto a = Slice<Tree>(tree);
    if (a.size() != 2) {
        raise_LengthRejected(Type::of<AnyVal>(), 2, 2, a.size());
    }
    Type type = Type(Str(a[0]));
    scheme->validate_type(type);
     // accepts_type might be overridden without this being overridden, so check
     // it.
    validate_type(type);
    expect(!value);
    value = AnyVal(type);
     // Run item_from_tree on the AnyVal's value, not on the AnyVal itself.
     // Otherwise, the associated routes will have an extra +1 in the fragment.
    item_from_tree(value.ptr(), a[1], SharedRoute(res), FromTreeOptions::DelaySwizzle);
}

UniqueArray<u8> ResourceExtension::to_blob (
    const AnyVal& v, ResourceRef res, PrintOptions opts
) {
    auto type_str = v.type.name();
    auto value_tree = item_to_tree(v.ptr(), SharedRoute(res));
    auto str = tree_to_string_for_file(
        Tree::array(Tree(type_str), move(value_tree)), opts
    );
    return UniqueArray<u8>(move(str));
}

void ResourceExtension::activate (const SharedString& name) noexcept {
    for (auto c : name) {
        require((c < 'A' || c > 'Z') && c != '.' && c != '/');
    }
    usize hash = uni::hash(name);
    auto& exts = g_universe->extensions;
    for (auto& entry : exts) {
        if (entry.hash == hash && entry.name == name) {
            require(false);
        }
    }
    exts.emplace_back(hash, name, this);
}

void ResourceExtension::activate_default () noexcept {
    require(!g_universe->default_extension);
    g_universe->default_extension = this;
}

void ResourceExtension::deactivate () noexcept {
    auto& exts = g_universe->extensions;
    u32 i = 0;
    while (i < exts.size()) {
        if (exts[i].extension == this) {
            exts.erase(i);
        }
        else i++;
    }
    if (g_universe->default_extension == this) {
        g_universe->default_extension = null;
    }
}

static constinit ResourceExtension default_default_extension;

ResourceExtension* get_extension (const IRI& name) {
     // TODO: lowercase!
    Str ext = iri::path_extension(name.path());
    if (auto& exts = g_universe->extensions) {
        auto hash = uni::hash(ext);
        for (auto& entry : exts) {
            if (entry.hash == hash && entry.name == ext) {
                return entry.extension;
            }
        }
        if (g_universe->default_extension) {
            return g_universe->default_extension;
        }
        else return null;
    }
    else if (g_universe->default_extension) {
        return g_universe->default_extension;
    }
    else return &default_default_extension;
}

ResourceExtension* require_extension (const IRI& name) {
    auto r = get_extension(name);
    if (!r) raise(e_ResourceExtensionNotFound, "This resource's extension has no handler");
    return r;
}

} // ayu
