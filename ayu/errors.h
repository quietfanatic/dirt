#pragma once

#include "common.h"
#include "location.h"
#include "print.h"
#include "resource.h"
#include "tree.h"
#include "type.h"

namespace ayu {

///// GENERAL

 // General IO-related problem
struct IOError : Error {
    AnyString filename;
    int errnum; // TODO: use std::errc
    IOError (AnyString f, int e) : filename(move(f)), errnum(e) { }
};
 // Failure to open a file.
struct OpenFailed : IOError {
    AnyString mode;
    OpenFailed (AnyString f, int e, AnyString m) :
        IOError(move(f), e), mode(move(m))
    { }
};
 // Failure to close a file
struct CloseFailed : IOError { using IOError::IOError; };

///// document.h

 // General category of errors coming from ayu::Document
 // TODO: Add a Document* to this
struct DocumentError : Error { };
 // Tried to create a document item with an illegal name.
struct DocumentInvalidName : DocumentError {
    AnyString name;
    DocumentInvalidName (AnyString n) : name(move(n)) { }
};
 // Tried to create a document item with a name that's already in use in
 // this document.
struct DocumentDuplicateName : DocumentError {
    AnyString name;
    DocumentDuplicateName (AnyString n) : name(move(n)) { }
};
 // Tried to delete a document item, but the wrong type was given during
 // deletion.
struct DocumentDeleteWrongType : DocumentError {
    Type existing;
    Type deleted_as;
    DocumentDeleteWrongType (Type e, Type d) : existing(e), deleted_as(d) { }
};
 // Tried to delete a document item by name, but the given name isn't in
 // this document.
struct DocumentDeleteMissing : DocumentError {
    AnyString name;
    DocumentDeleteMissing (AnyString n) : name(move(n)) { }
};

///// location.h

 // The given IRI could not be transformed into a location (probably doesn't
 // have the correct syntax in its #fragment).
struct InvalidLocationIRI : Error {
    AnyString spec;
    StaticString mess;
    InvalidLocationIRI (AnyString s, StaticString m) :
        spec(move(s)), mess(m)
    { }
};

///// parse.h

struct ParseError : Error {
    AnyString mess;
    AnyString filename;
    uint line;
    uint col;
    ParseError (AnyString m, AnyString f, uint l, uint c) :
        mess(move(m)), filename(move(f)), line(l), col(c)
    { }
};
 // Failure to read from an open file
struct ReadFailed : IOError { using IOError::IOError; };

///// print.h

 // Conflicting combination of print options was provided, or it had bits
 // outside of VALID_PRINT_OPTION_BITS.
struct InvalidPrintOptions : Error {
    PrintOptions options;
    InvalidPrintOptions (PrintOptions o) : options(o) { }
};
 // Failure to write to an open file
struct WriteFailed : IOError { using IOError::IOError; };

///// reference.h

struct ReferenceError : Error {
    Location location;
    Type type;
    ReferenceError (Location l, Type t) : location(move(l)), type(t) { }
};
 // Tried to write to a readonly reference.
struct WriteReadonlyReference : ReferenceError { using ReferenceError::ReferenceError; };
 // Used the reference in a context where its address was required, but it
 // has no address.
struct UnaddressableReference : ReferenceError { using ReferenceError::ReferenceError; };

///// resource-scheme.h

struct ResourceNameError : Error { };
 // An invalid IRI was given as a resource name.
struct InvalidResourceName : ResourceNameError {
    AnyString name;
    InvalidResourceName (AnyString n) : name(move(n)) { }
};
 // Tried to use an IRI as a resource name but its scheme was not registered
struct UnknownResourceScheme : ResourceNameError {
    AnyString name;
    UnknownResourceScheme (AnyString n) : name(move(n)) { }
};
 // A valid IRI was given but its ResourceScheme didn't like it.
struct UnacceptableResourceName : ResourceNameError {
    AnyString name;
    UnacceptableResourceName (AnyString n) : name(move(n)) { }
};
 // Tried to load or set_value a resource with a type that the
 // ResourceScheme didn't accept.
struct UnacceptableResourceType : ResourceNameError {
    AnyString name;
    Type type;
    UnacceptableResourceType (AnyString n, Type t) : name(move(n)), type(t) { }
};
 // Tried to register a ResourceScheme with an invalid name.
struct InvalidResourceScheme : ResourceNameError {
    AnyString scheme;
    InvalidResourceScheme (AnyString n) : scheme(move(n)) { }
};
 // Tried to register multiple ResourceSchemes with the same name.
struct DuplicateResourceScheme : ResourceNameError {
    AnyString scheme;
    DuplicateResourceScheme (AnyString n) : scheme(move(n)) { }
};

///// resource.h

struct ResourceError : Error { };
 // Tried an an operation on a resource when its state wasn't appropriate
 // for that operation.
struct InvalidResourceState : ResourceError {
    StaticString tried;
    Resource resource;
    ResourceState state;
    InvalidResourceState (StaticString t, Resource r) :
        tried(t), resource(r), state(r.state())
    { }
};
 // Tried to create a resource with an empty value.
struct EmptyResourceValue : ResourceError {
    AnyString name;
    EmptyResourceValue (AnyString n) : name(move(n)) { }
};
 // Tried to unload a resource, but there's still a reference somewhere
 // referencing an item inside it.
struct UnloadBreak {
    Location from;
    Location to;
};
struct UnloadWouldBreak : ResourceError {
    UniqueArray<UnloadBreak> breaks;
    UnloadWouldBreak (UniqueArray<UnloadBreak>&& b) : breaks(b) { }
};
 // Tried to reload a resource, but was unable to update a reference
 // somewhere.
struct ReloadBreak {
    Location from;
    Location to;
    std::exception_ptr inner;
};
struct ReloadWouldBreak : ResourceError {
    UniqueArray<ReloadBreak> breaks;
    ReloadWouldBreak (UniqueArray<ReloadBreak>&& b) : breaks(b) { }
};
 // Failed to delete a resource's source file.
struct RemoveSourceFailed : ResourceError {
    Resource resource;
    int errnum; // errno
    RemoveSourceFailed (Resource r, int e) : resource(r), errnum(e) { }
};

///// scan.h

 // Required the location of a reference, but a global scan or cache lookup
 // couldn't find it.
struct ReferenceNotFound : Error {
     // We can't stuff the reference in here because the error message will try
     // to call reference_to_location on it, consuming infinite stack.
     // TODO: Is there any more information we can stuff in here?  Would a void*
     // be useful?
    Type type;
    ReferenceNotFound (Type t) : type(t) { }
};

///// serialize.h

 // A serialization operation failed.  The inner exception can be anything, but
 // it's likely to be one of other serialize.h errors below.
struct SerializeFailed : Error {
    Location location;
    Type type;
    std::exception_ptr inner;
    SerializeFailed (Location l, Type t, std::exception_ptr i) :
        location(move(l)), type(t), inner(move(i))
    { }
};

 // Tried to call to_tree on a type that doesn't support to_tree
struct ToTreeNotSupported : Error { };
 // Tried to call from_tree on a type that doesn't support from_tree
struct FromTreeNotSupported : Error { };
 // Tried to deserialize an item from a tree, but the item didn't accept
 // the tree's form.
struct InvalidForm : Error {
    TreeForm form;
    InvalidForm (TreeForm f) : form(f) { }
};
 // Tried to serialize an item using a values() descriptor, but no value()
 // entry was found for the item's current value.
struct NoNameForValue : Error { };
 // Tried to deserialize an item using a values() descriptor, but no value()
 // entry was found that matched the provided name.
struct NoValueForName : Error {
    Tree name;
    NoValueForName (Tree n) : name(n) { }
};
 // Tried to deserialize an item from an object tree, but the tree lacks an
 // attribute that the item requires.
struct MissingAttr : Error {
    AnyString key;
    MissingAttr (AnyString k) : key(move(k)) { }
};
 // Tried to deserialize an item from an object tree, but the item rejected
 // one of the attributes in the tree.
struct UnwantedAttr : Error {
    AnyString key;
    UnwantedAttr (AnyString k) : key(move(k)) { }
};
 // Tried to deserialize an item from an array tree, but the array has too
 // few or too many elements for the item.
struct WrongLength : Error {
    usize min;
    usize max;
    usize got;
    WrongLength (usize mi, usize ma, usize g) : min(mi), max(ma), got(g) { }
};
 // Tried to treat an item like it has attributes, but it does not support
 // behaving like an object.
struct NoAttrs : Error { };
 // Tried to treat an item like it has elements, but it does not support
 // behaving like an array.
struct NoElems : Error { };
 // Tried to get an attribute from an item, but it doesn't have an attribute
 // with the given key.
struct AttrNotFound : Error {
    AnyString key;
    AttrNotFound (AnyString k) : key(move(k)) { }
};
 // Tried to get an element from an item, but it doesn't have an element
 // with the given index (the index is out of bounds).
struct ElemNotFound : Error {
    usize index;
    ElemNotFound (usize i) : index(i) { }
};
 // The accessor given to a keys() descriptor did not serialize to an array
 // of strings.
struct InvalidKeysType : Error {
    Type keys_type;
    InvalidKeysType (Type k) : keys_type(k) { }
};

///// tree.h

struct TreeError : Error { };
 // Tried to treat a tree as though it's a form which it's not.
struct WrongForm : TreeError {
    TreeForm form;
    Tree tree;  // TODO: take TreeForm not Tree
    WrongForm (TreeForm f, Tree t) : form(f), tree(move(t)) { }
};
 // Tried to extract a number from a tree, but the tree's number won't fit
 // into the requested type.
 // TODO: Cannot
struct CantRepresent : TreeError {
    StaticString type_name;
    Tree tree;
    CantRepresent (StaticString n, Tree t) : type_name(n), tree(move(t)) { }
};

///// type.h

struct TypeError : Error { };
 // Tried to map a C++ type to an AYU type, but AYU doesn't know about this
 // type (it has no AYU_DESCRIBE description).
 // TODO: serializing this doesn't work?
struct UnknownType : TypeError {
    const std::type_info* cpp_type;
    UnknownType (const std::type_info& c) : cpp_type(&c) { }
};
 // Tried to look up a type by name, but there is no type with that name.
struct TypeNotFound : TypeError {
    AnyString name;
    TypeNotFound (AnyString n) : name(move(n)) { }
};
 // Tried to default construct a type that has no default constructor.
struct CannotDefaultConstruct : TypeError {
    Type type;
    CannotDefaultConstruct (Type t) : type(t) { }
};
 // Tried to construct or destroy a type that has no destructor.
struct CannotDestroy : TypeError {
    Type type;
    CannotDestroy (Type t) : type(t) { }
};
 // Tried to coerce between types that can't be coerced.
struct CannotCoerce : TypeError {
    Type from;
    Type to;
    CannotCoerce (Type f, Type t) : from(f), to(t) { }
};

} // ayu
