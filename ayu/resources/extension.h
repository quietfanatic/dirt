 // A ResourceExtension (as in filename extension) tells how to transform a file
 // into an item and vice versa.  Normally files are transformed with
 // item_from_string, but you can change that with a ResourceExtension.  An
 // example use case for this is loading an image file directly into a
 // UniqueImage.
 //
 // ResourceExtensions are selected only by filename extension, not by magic
 // number.
 //
 // If no ResourceExtensions are active, a default ResourceExtension is provided
 // that matches all files and performs the default behavior.

#pragma once
#include "../data/print.h"
#include "scheme.h"

namespace ayu {

[[noreturn, gnu::cold]]
void raise_ResourceTypeRejectedByExtension (Type);

struct ResourceExtension {
     // Restrict the types this handler can work with.  This is called if
     // set_value() is called on the Resource.  It's also called before
     // to_blob().  It is not called on the output of from_blob.  You are
     // responsible for producing your own acceptable type.
    virtual bool accepts_type (Type) { return true; }

    void validate_type (Type t) {
        if (!accepts_type(t)) raise_ResourceTypeRejectedByExtension(t);
    }

     // How to load this kind of item.  You don't have to read the file, as it
     // is already read into a blob.
     //
     // You must call scheme->validate_type at some point, preferably as soon as
     // you know what type you're going to produce.  If you expect to be
     // subclassed, you should also call this->validate_type, since the subclass
     // might override accepts_type but not from_blob.
     //
     // The default implementation of this function calls item_from_string.  If
     // you call item_from_string or item_from_tree yourself, you should do it
     // in-place on the value.ptr(), instead of creating a new AnyVal and moving
     // it onto value.  This will help error messages find the location of the
     // error.
     //
     // If you'd rather have a character string than a binary blob, do this.
     //
     //     auto contents = Str(blob);
     //
    virtual void from_blob (
        AnyVal& value, Slice<u8> blob, ResourceRef res, ResourceScheme* scheme
    );

     // How to save this kind of item.  Don't actually write the file!  Return
     // the contents of the file.  This may be run under a transaction, where
     // the actual writing of the file is delayed until the transaction commits.
     //
     // The provided AnyVal is guaranteed to be something for which accepts_type
     // returns true.
     //
     // The provided PrintOptions are whatever were passed to save().  You may
     // ignore them.
     //
     // The default implementation of this function calls item_to_string.
     //
     // If you'd rather produce a character string than a binary blob, do this.
     //
     //     UniqueString contents = ...;
     //     return UniqueArray<u8>(move(contents));
     //
    virtual UniqueArray<u8> to_blob (
        const AnyVal& value, ResourceRef res, PrintOptions opts
    );

    constexpr ResourceExtension () { }
    explicit ResourceExtension (const SharedString& name) { activate(name); }

     // No copy or move
    ResourceExtension (const ResourceExtension&) = delete;
    ResourceExtension (ResourceExtension&& o) = delete;
    ResourceExtension& operator = (const ResourceExtension&) = delete;
    ResourceExtension& operator = (ResourceExtension&&) = delete;

    virtual ~ResourceExtension () { deactivate(); }

     // The name you activate this with must contain no /s or .s and all ascii
     // characters must be lowercase.  The empty string matches files with no
     // extension.  You cannot have two extensions with the same name.
    void activate (const SharedString&) noexcept;
     // Register this as the default extension.  It will handle all resources
     // that don't have any other extension registered.
    void activate_default () noexcept;
     // Deactivating deactivates both named activations and default activations.
    void deactivate () noexcept;
};

 // Returns null if not found
ResourceExtension* get_extension (const IRI&);
 // Throws ResourceExtensionNotFound if not found
ResourceExtension* require_extension (const IRI&);

inline constexpr ErrorCode e_ResourceExtensionNotFound = "ayu::e_ResourceExtensionNotFound";

} // ayu
