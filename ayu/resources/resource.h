 // A resource represents a top-level named piece of program data.  A
 // resource has:
 //     - a source, which is by default a file on disk
 //     - a name, which in the case of files, is essentially its file path
 //     - a value, which is a Dynamic
 //     - a state, which is usually UNLOADED or LOADED.
 // Resources can be loaded, reloaded, unloaded, and saved.
 //
 // Resource names are an IRI.
 //
 // Resources can have no name, in which case they are anonymous.  Anonymous
 // resources cannot be reloaded or saved, and they can be unloaded but once
 // they are unloaded they can never be loaded again.  Anonymous resources can
 // contain references to named resources, and those references will be updated
 // if those resources are reloaded.  Named resources cannot be saved if they
 // contain references to anonymous resources, because there's no way to
 // serialize that reference as a path.  TODO: actually implement this
 //
 // So, if you have global variables that reference things in resources, make
 // those global variables anonymous resources, and they will be automatically
 // updated whenever the resource is reloaded.
 //
 // The Resource class itself can be constructed at init time, but Resources
 // cannot be loaded or have their value set until main() starts.

#pragma once

#include "../common.h"
#include "../reflection/reference.h"

namespace ayu {

///// RESOURCES

enum ResourceState {
     // The resource is not loaded and has an empty value.
    UNLOADED,
     // This resource is fully loaded and has a non-empty up-to-date value,
     // though that value may not reflect what is on disk.
    LOADED,

     // The following states will only be encountered while an ayu resource
     // operation is ongoing.

     // load() is being called on this resource.  Its value may be partially
     // constructed.
    LOAD_CONSTRUCTING,
     // load() is being called on this resource, but there was an error, so
     // its destructor is being or will be called.
    LOAD_ROLLBACK,
     // save() is being called on this resource, but it is not being written to
     // disk yet.
    SAVE_VERIFYING,
     // save() is being called on this resource, and it is being or will be
     // written to disk.
    SAVE_COMMITTING,
     // unload() is being called on this resource, and other resources are being
     // scanned for references to it.
    UNLOAD_VERIFYING,
     // unload() is being called on this resource, and its destructor is being
     // or will be called.  There is no UNLOAD_ROLLBACK because unload doesn't
     // need to roll anything back.
    UNLOAD_COMMITTING,
     // reload() is being called on this resource, and its new value is being
     // constructed.  value() will return its (maybe incomplete) new value.
    RELOAD_CONSTRUCTING,
     // reload() is being called on this resource, and other resources are being
     // scanned for references to update.
    RELOAD_VERIFYING,
     // reload() is being called on this resource, but there was an error, so
     // its new value is being destructed and its old value will be restored.
    RELOAD_ROLLBACK,
     // reload() is being called on this resource, and its old value is being
     // destructed.
    RELOAD_COMMITTING,
};
 // Get the string name of a resource state.
StaticString show_ResourceState (ResourceState) noexcept;

 // The Resource class refers to a resource with reference semantics.
 // This class itself is cheap to copy.
struct Resource {
     // Internal data is kept perpetually, but may be refcounted at some point
     // (it's about 7-10 words long)
    in::ResourceData* data;

    constexpr Resource (in::ResourceData* d = null) : data(d) { }
     // Refers to the resource with this name, but does not load it yet.
    Resource (const IRI& name);
     // Takes an IRI reference relative to the current resource if there is one.
    Resource (Str name);
     // Too long of a conversion chain for C++ lol
    template <usize n>
    Resource (const char (& name)[n]) : Resource(Str(name)) { }
     // Creates the resource already loaded with the given data, without reading
     // from disk.  Will throw InvalidResourceState if a resource with this
     // name is already loaded or EmptyResourceValue if value is empty.
    Resource (const IRI& name, MoveRef<Dynamic> value);
     // Creates an anonymous resource with the given value
     // TODO: I forgot to implement this
    Resource (Null, MoveRef<Dynamic> value);

     // Returns the resource's name as an IRI
    const IRI& name () const noexcept;
     // See enum ResourceState
    ResourceState state () const noexcept;

     // If the resource is UNLOADED, automatically loads the resource from disk.
     // Will throw if autoloading the resource but the file does not exist.
    Dynamic& value () const;
     // Gets the value without autoloading.  If the result is empty, do not
     // write to it.  Nothing bad will happen immediately, but the value will
     // be overwritten when the resource is loaded.
    Dynamic& get_value () const noexcept;
     // If the resource is UNLOADED, sets is state to LOADED without loading
     // from disk, and sets its value.  Throws InvalidResourceState if the
     // resource's state is anything but UNLOADED, LOADED, or LOAD_CONSTRUCTING.
     // Throws UnacceptableResourceType if this resource has a name and the
     // ResourceScheme associated with its name returns false from accepts_type.
    void set_value (MoveRef<Dynamic>) const;

     // Automatically loads and returns a reference to the value, which can be
     // coerced to a pointer.
    Reference ref () const;
     // Gets a reference to the value without automatically loading.  If the
     // resource is UNLOADED, returns an empty Reference.
    Reference get_ref () const noexcept;

     // Syntax sugar
    explicit operator bool () const { return data; }
    auto operator [] (const AnyString& key) { return ref()[key]; }
    auto operator [] (usize index) { return ref()[index]; }
};

 // Resources are considered equal if their names are equal (Resources with the
 // same name will always have the same data pointer).
 // Automatic coercion is disabled for these operators.
template <class T> requires (std::is_same_v<T, Resource>)
inline bool operator == (T a, T b) { return a.data == b.data; }
template <class T> requires (std::is_same_v<T, Resource>)
inline bool operator != (T a, T b) { return !(a == b); }

///// RESOURCE OPERATIONS

 // Loads a resource.  Does nothing if the resource is not UNLOADED.  Throws
 // if the file doesn't exist on disk or can't be opened.
void load (Resource);
 // Loads multiple resources at once.  If an exception is thrown, all the loads
 // will be cancelled and all of the given resources will end up in the UNLOADED
 // state (unless they were already LOADED beforehand).
void load (Slice<Resource>);

 // Moves old_res's value to new_res.  Does not change the names of any Resource
 // objects, just the mapping from names to values.  Does not affect any files
 // on disk.  Will throw if old_res is not LOADED, or if new_res is not
 // UNLOADED.  After renaming, old_res will be UNLOADED and new_res will
 // be LOADED.
void rename (Resource old_res, Resource new_res);

 // Saves a loaded resource to disk.  Throws if the resource is not LOADED.  May
 // overwrite an existing file on disk.
void save (Resource);
 // Saves multiple resources at once.  This will be more efficient than saving
 // them one at a time (the universe only needs to be scanned once for
 // serializing references).  If an error is thrown, none of the resources will
 // be saved.  (Exception: there may be some cases where if an error is thrown
 // while writing to disk, the on-disk resources may be left in an inconsistent
 // state.)
void save (Slice<Resource>);

 // Clears the value of the resource and sets its state to UNLOADED.  Does
 // nothing if the resource is UNLOADED, and throws if it is LOADING.  Scans all
 // other LOADED resources to make sure none of them are referencing this
 // resource, and if any are, this call will throw UnloadWouldBreak and the
 // resource will not be unloaded.
void unload (Resource);
 // Unloads multiple resources at once.  This will be more efficient than
 // unloading them one at a time (the universe only needs to be scanned once),
 // and if two resources have references to eachother, they have to be unloaded
 // at the same time.  If unloading any of the resources causes an exception,
 // none of the references will be unloaded.
void unload (Slice<Resource>);

 // Immediately unloads the file without scanning for references to it.  This is
 // faster, but if there are any references to data in this resource, they will
 // be left dangling.
void force_unload (Resource);
void force_unload (Slice<Resource>);

 // Reloads a resource that is loaded.  Throws if the resource is not LOADED.
 // Scans all other resources for references to this one and updates them to
 // the new address.  If the reference is no longer valid, this call will throw
 // ReloadWouldBreak, and the resource will be restored to its old value
 // before the call to reload.  It is an error to reload while another resource
 // is being loaded.
void reload (Resource);
 // Reloads multiple resources at once.  This wlil be more efficient than
 // reloading them one at a time.  If an exception is thrown for any of the
 // resources, all of them will be restored to their old value before the call
 // to reload.
void reload (Slice<Resource>);

 // Deletes the source of the resource.  If the source is a file, deletes the
 // file without confirmation.  Does not change the resource's state or value.
 // Does nothing if the source doesn't exist, but throws RemoveSourceFailed
 // if another error occurs (permission denied, etc.)  Calling load on the
 // resource should fail after this.
void remove_source (Resource);

 // Returns true if the given resource's file exists on disk.  Does a pretty
 // basic test: it tries to open the file, and returns true if it can or false
 // if it can't.
bool source_exists (Resource);

 // Get the filename of the file backing this resource, if it has one.
AnyString resource_filename (Resource);

 // Returns a list of all resources with state != UNLOADED.  This includes
 // resources that are in the process of being loaded, reloaded, or unloaded.
UniqueArray<Resource> loaded_resources () noexcept;

///// RESOURCE ERROR CODES

 // Tried to use an invalid IRI as a resource name
constexpr ErrorCode e_ResourceNameInvalid = "ayu::e_ResourceNameInvalid";
 // The ResourceScheme associated with the resource name rejected the name.
constexpr ErrorCode e_ResourceNameRejected = "ayu::e_ResourceNameRejected";
 // The ResourceScheme associated with the resource did not accept the type
 // provided for the resource.  This can happen either while loading from a
 // file, or when setting a resource's value programmatically.
constexpr ErrorCode e_ResourceTypeRejected = "ayu::e_ResourceTypeRejected";
 // Tried to create a resource with an empty Dynamic, or load from a tree that
 // didn't correspond to a valid Dynamic.
constexpr ErrorCode e_ResourceValueInvalid = "ayu::e_ResourceValueInvalid";
 // Tried to perform an operation on a resource, but its state() was not valid
 // for that operations.
constexpr ErrorCode e_ResourceStateInvalid = "ayu::e_ResourceStateInvalid";
 // Tried to unload a resource, but there's still a reference somewhere pointing
 // to an item inside it.
constexpr ErrorCode e_ResourceUnloadWouldBreak = "ayu::e_ResourceUnloadWouldBreak";
 // Tried to reload a resource, but there was a reference pointing to an item
 // inside of it which couldn't be updated for some reason.
constexpr ErrorCode e_ResourceReloadWouldBreak = "ayu::e_ResourceReloadWouldBreak";
 // Failed to delete a resource's source file.
constexpr ErrorCode e_ResourceRemoveSourceFailed = "ayu::e_ResourceRemoveSourceFailed";

} // namespace ayu
