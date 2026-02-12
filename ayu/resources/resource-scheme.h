// A resource name is an IRI.  Interpretation of IRIs is determined by
// globally-registered ResourceScheme objects, but generally they refer to
// files on disk.
//     scheme:/path/to/file.ayu

#pragma once
#include "../../iri/iri.h"
#include "../../iri/path.h"
#include "../common.h"
#include "../reflection/type.h"

namespace ayu {

 // More info will be added in tags.
[[noreturn, gnu::cold]]
void raise_ResourceNameRejected ();
[[noreturn, gnu::cold]]
void raise_ResourceTypeRejected (Type);
[[noreturn, gnu::cold]]
void raise_ResourceNoFilepath ();

 // Registers a resource scheme at startup.  The path parameter passed to all
 // the virtual methods is just the path part of the name, and is always
 // canonicalized and absolute.
 //
 // Currently, resources in a scheme are only allowed to link to other resources
 // in the same scheme.
 //
 // If no ResourceSchemes are active, then a default resource scheme with the
 // name "file" will be used, which maps resource names directly to files in the
 // filesystem.
 //
 // ResourceSchemes are allowed to be constructed at init time, but you can't
 // manipulate any Types until main() starts.
struct ResourceScheme {
     // Must be a valid scheme name matching [a-z][a-z0-9+.-]* and must not be
     // the same as any other ResourceScheme name, otherwise a runtime assert
     // will be triggered.
    const AnyString name;

     // If you want to do some of your own validation besides the standard IRI
     // validation.  If this returns false, ResourceNameRejected will be thrown.
     // The provided IRI will always be valid and will not have a #fragment.
    virtual bool accepts_name (const IRI&) { return true; }

    void validate_name (const IRI& name) {
        if (!accepts_name(name)) raise_ResourceNameRejected();
    }

     // If you want to limit the allowed top-level types of your resources.
     // This is called when load(), reload(), save(), or set_value() is called
     // on a resource of this scheme, or a resource of this scheme is
     // constructed with a specific provided value.  If this returns false,
     // ResourceTypeRejected will be thrown.
     //
     // TODO: provide name as well?
    virtual bool accepts_type (Type) { return true; }

    void validate_type (Type t) {
        if (!accepts_type(t)) raise_ResourceTypeRejected(t);
    }

     // Turn an IRI into a filename.  If "" is returned, it means there is no
     // valid filename for this IRI, and ResourceNoFilepath is likely to be
     // thrown soon.  It is okay to name a non-existent file: that file will be
     // created when the resource is saved.
     //
     // TODO: Non-file resource schemes
    virtual AnyString get_filepath (const IRI&) { return ""; }

    AnyString require_filepath (const IRI& name) {
        AnyString r = get_filepath(name);
        if (!r) raise_ResourceNoFilepath();
        return r;
    }

     // By default, constructing a ResourceScheme will activate it (which is not
     // constexprable).
    constexpr explicit
    ResourceScheme (AnyString n, bool auto_activate = true) :
        name(move(n))
    {
        if (auto_activate) activate();
    }

    ResourceScheme (const ResourceScheme&) = delete;
    ResourceScheme (ResourceScheme&& o) = delete;
    ResourceScheme& operator = (const ResourceScheme&) = delete;
    ResourceScheme& operator = (ResourceScheme&&) = delete;

    virtual ~ResourceScheme () { deactivate(); }

     // These are called in the constructor (by default) and destructor, so you
     // don't have to call them yourself.
    void activate ();
    void deactivate () noexcept;
};

 // May throw ResourceSchemeNotFound
ResourceScheme* require_scheme (const IRI& name);

 // Tried to find a resoursce scheme that didn't exist.
constexpr ErrorCode e_ResourceSchemeNotFound = "ayu::e_ResourceSchemeNotFound";
 // The ResourceScheme associated with the resource name rejected the name.
constexpr ErrorCode e_ResourceNameRejected = "ayu::e_ResourceNameRejected";
 // The ResourceScheme associated with the resource name did not provide a
 // filepath for its name.
constexpr ErrorCode e_ResourceNoFilepath = "ayu::e_ResourceNoFilepath";
 // The ResourceScheme associated with the resource did not accept the type
 // provided for the resource.  This can happen either while loading from a
 // file, or when setting a resource's value programmatically.
constexpr ErrorCode e_ResourceTypeRejected = "ayu::e_ResourceTypeRejected";

 // Maps resource names to the contents of a folder.
struct FolderResourceScheme : ResourceScheme {
     // Must be a file:/ IRI
    IRI folder;

    virtual bool accepts_name (const IRI& iri) override {
        return !iri.has_authority() && !iri.has_query()
            && iri.hierarchical();
    }

    virtual AnyString get_filepath (const IRI& iri) override {
        if (!iri.hierarchical()) return "";
        IRI abs = IRI(iri.path().slice(1), folder);
        return iri::to_fs_path(abs);
    }

    FolderResourceScheme (
        AnyString n, Str folder, bool auto_activate = true
    ) :
        ResourceScheme(move(n), auto_activate),
        folder(iri::from_fs_path(cat(folder, '/')))
    { }

    constexpr FolderResourceScheme (
        AnyString n, const IRI& folder, bool auto_activate = true
    ) :
        ResourceScheme(move(n), auto_activate),
        folder(folder)
    {
        require(
            folder.scheme() == "file" &&
            folder.hierarchical() &&
            folder.path().back() == '/'
        );
    }
};

} // namespace ayu
