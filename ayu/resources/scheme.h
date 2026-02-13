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
 // If no ResourceSchemes are active, then a default FolderResourceScheme with
 // the name "file" will be used, which maps resource names directly to files in
 // the filesystem.
 //
 // ResourceSchemes are allowed to be constructed and activated at init time,
 // but you can't use them until main starts.
struct ResourceScheme {
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

    constexpr ResourceScheme () { }

     // Constructing with a name automatically activates the scheme.  The name
     // must be a valid IRI scheme matching [a-z][a-z0-9+.-]* and must not be
     // the same as any other ResourceScheme name, otherwise a runtime assert
     // will be triggered.
    explicit
    ResourceScheme (const AnyString& name) { activate(name); }

     // Cannot copy or move ResourceScheme because its address must remain
     // fixed.
    ResourceScheme (const ResourceScheme&) = delete;
    ResourceScheme (ResourceScheme&& o) = delete;
    ResourceScheme& operator = (const ResourceScheme&) = delete;
    ResourceScheme& operator = (ResourceScheme&&) = delete;

     // Destroying a ResourceScheme automatically deactivates it.
    virtual ~ResourceScheme () { deactivate(); }

     // You can activate a ResourceScheme under multiple scheme names if you
     // want.  This is less useful than it is for ResourceExtension.
    void activate (const AnyString&) noexcept;
     // Deactivating a ResourceScheme deactivates it for all names it was
     // activated with.  If you deactivate a ResourceScheme while a Resource
     // that uses it is loaded, you will not be able to save or reload the
     // Resource, but you will be able to unload it.  Reactivating the
     // ResourceScheme will fix this.  You could also activate a different
     // ResourceScheme with the same name, but why would you do that.
    void deactivate () noexcept;
};

 // Returns null if there's no scheme
ResourceScheme* get_scheme (const IRI& name);
 // May throw ResourceSchemeNotFound
ResourceScheme* require_scheme (const IRI& name);

///// ERROR CODES

 // Tried to find a resource scheme that didn't exist.
constexpr ErrorCode e_ResourceSchemeNotFound = "ayu::e_ResourceSchemeNotFound";
 // The ResourceScheme associated with the resource name rejected the name.
constexpr ErrorCode e_ResourceNameRejected = "ayu::e_ResourceNameRejected";
 // The ResourceScheme associated with the resource did not accept the type
 // provided for the resource.  This can happen either while loading from a
 // file, or when setting a resource's value programmatically.
constexpr ErrorCode e_ResourceTypeRejected = "ayu::e_ResourceTypeRejected";
 // The ResourceScheme associated with the resource name did not provide a
 // filepath for its name.
constexpr ErrorCode e_ResourceNoFilepath = "ayu::e_ResourceNoFilepath";

///// FOLDER RESOURCE SCHEMES

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

     // Constructing from an IRI can be constexpr
    constexpr FolderResourceScheme (const IRI& f) :
        folder(f)
    {
        require(
            folder.scheme() == "file" &&
            folder.hierarchical() &&
            folder.path().back() == '/'
        );
    }
     // (but not if you auto-activate it)
    explicit FolderResourceScheme (const AnyString& n, const IRI& f) :
        FolderResourceScheme(f)
    { activate(n); }

     // Construct with an OS path to a folder.
    FolderResourceScheme (Str f) :
        FolderResourceScheme(iri::from_fs_path(cat(f, '/')))
    { }
     // (with auto-activate)
    explicit FolderResourceScheme (const AnyString& n, Str f) :
        FolderResourceScheme(f)
    { activate(n); }
};

} // namespace ayu
