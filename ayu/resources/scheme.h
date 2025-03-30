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

 // Registers a resource scheme at startup.  The path parameter passed to all
 // the virtual methods is just the path part of the name, and is always
 // canonicalized and absolute.
 //
 // Currently, resources from a scheme are only allowed to reference other
 // resources from the same scheme.
 //
 // If no ResourceSchemes are active, then a default resource scheme with the
 // name "file" will be used, which maps resource names to files on disk.
 //
 // ResourceSchemes are allowed to be constructed at init time, but you can't
 // manipulate any Types until main() starts.
struct ResourceScheme {
     // Must be a valid scheme name matching [a-z][a-z0-9+.-]*
    const AnyString name;

     // If you want to do some of your own validation besides the standard IRI
     // validation.  If this returns false, UnacceptableResourceName will
     // be thrown.  The provided IRI will always be valid and will not have a
     // #fragment.
    virtual bool accepts_name (const IRI&) const { return true; }
     // If you want to limit the allowed top-level types of your resources.
     // This is called when load(), reload(), save(), or set_value() is called
     // on a resource of this scheme, or a resource of this scheme is
     // constructed with a specific provided value.  If this returns false,
     // UnacceptableResourceType will be thrown.
     // TODO: provide name as well
    virtual bool accepts_type (Type) const { return true; }
     // Turn an IRI into a filename.  If "" is returned, it means there is no
     // valid filename for this IRI.  It is okay to return non-existent
     // filenames.
    virtual AnyString get_file (const IRI&) const { return ""; }
     // TODO: Non-file resource schemes

    explicit ResourceScheme (AnyString n, bool auto_activate = true) :
        name(move(n))
    {
        if (auto_activate) activate();
    }

    ResourceScheme (const ResourceScheme&) = delete;
    ResourceScheme (ResourceScheme&& o) = delete;
    ResourceScheme& operator = (const ResourceScheme&) = delete;
    ResourceScheme& operator = (ResourceScheme&&) = delete;

    virtual ~ResourceScheme () {
        deactivate();
    }

     // These are called in the constructor (by default) and destructor, so you
     // don't have to call them yourself.
    void activate () const;
    void deactivate () const noexcept;
};

ResourceScheme* get_resource_scheme (Str name);

 // The name for a resource scheme was not a valid IRI scheme.
constexpr ErrorCode e_ResourceSchemeNameInvalid = "ayu::e_ResourceSchemeNameInvalid";
 // Tried to register a scheme name that was already registered.
constexpr ErrorCode e_ResourceSchemeNameDuplicate = "ayu::e_ResourceSchemeNameDuplicate";
 // Tried to find a resoursce scheme that didn't exist.
constexpr ErrorCode e_ResourceSchemeNotFound = "ayu::e_ResourceSchemeNotFound";

 // Maps resource names to the contents of a folder.
struct FolderResourceScheme : ResourceScheme {
     // Must be a file:/ IRI
    IRI folder;

    bool accepts_name (const IRI& iri) const override {
        return !iri.has_authority() && !iri.has_query()
            && iri.hierarchical();
    }

    AnyString get_file (const IRI& iri) const override {
        require(iri.hierarchical());
        IRI abs = IRI(iri.path().slice(1), folder);
        return iri::to_fs_path(abs);
    }

    FolderResourceScheme (
        AnyString n, Str folder, bool auto_activate = true
    ) :
        ResourceScheme(move(n), auto_activate),
        folder(iri::from_fs_path(cat(folder, '/')))
    { }

    FolderResourceScheme (
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
