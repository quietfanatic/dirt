 // A resource represents a top-level named piece of program data.  A
 // resource has:
 //     - a source, which is by default a file on disk
 //     - a name, which in the case of files, is essentially its file path
 //     - a value, which is a Dynamic
 //     - a state, which is usually RS::Unloaded or RS::Loaded.
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
#include "../../uni/transaction.h"

namespace ayu {

///// RESOURCES

 // The possible states a Resource can have.  During normal program operation,
 // Resources will only be RS::Unloaded or RS::Loaded.  The other states will only occur
 // while a resource operation is actively happening, or while a
 // ResourceTransaction is open.
enum class ResourceState : uint8 {
     // The resource is not loaded and has an empty value.
    Unloaded,
     // This resource is fully loaded and has a non-empty up-to-date value,
     // though that value may not reflect what is on disk.
    Loaded,

     // load() is being called on this resource.  Its value may be partially
     // constructed.
    LoadConstructing,
     // load() has been called on this resource and it is fully constructed, but
     // we're in a transaction, so the load may be rolled back if another
     // operation fails.
    LoadReady,
     // load() was being called on this resource, but there was an error, so
     // its destructor is being called.
    LoadCancelling,

     // unload() is being called on this resource, and other resources are being
     // scanned for references to it to make sure it's safe to unload.
    UnloadVerifying,
     // unload() was called on this resource, but we're in a transaction, so the
     // unload might be cancelled if another operation fails.
    UnloadReady,
     // unload() was being called on this resource, and its destructor is being
     // or will be called.
    UnloadCommitting,

     // reload() is being called on this resource, and its new value is being
     // constructed.  value() will return its (maybe incomplete) new value.
    ReloadConstructing,
     // reload() is being called on this resource, and other resources are being
     // scanned for references to update.
    ReloadVerifying,
     // reload() was called on this resource, but we're in a transaction and the
     // reload will be cancelled if another operation fails.
    ReloadReady,
     // reload() was called on this resource, but there was an error, so
     // its new value is being destructed and its old value will be restored.
    ReloadCancelling,
     // reload() was being called on this resource, and its old value is being
     // destructed.
    ReloadCommitting,
};
using RS = ResourceState;
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
     // Too long of a conversion chain for C++ I guess
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

     // If the resource is RS::Unloaded, automatically loads the resource from disk.
     // Will throw if the load fails.  If a ResourceTransaction is currently
     // active, the value will be cleared if the ResourceTransaction is rolled
     // back.
    Dynamic& value () const;
     // Gets the value without autoloading.  Writing to this is Undefined
     // Behavior if the state is anything except for RS::Loaded or RS::LoadReady.
    Dynamic& get_value () const noexcept;
     // If the resource is RS::Unloaded, sets is state to RS::Loaded or RS::LoadReady
     // without loading from disk, and sets its value.  Throws
     // ResourceStateInvalid if the resource's state is anything but RS::Unloaded,
     // RS::Loaded, or RS::LoadReady.  Throws ResourceTypeRejected if this resource has
     // a name and the ResourceScheme associated with its name returns false
     // from accepts_type.
    void set_value (MoveRef<Dynamic>) const;

     // Automatically loads and returns a reference to the value, which can be
     // coerced to a pointer.  If a ResourceTransaction is currently active, the
     // value will be cleared if the transaction is rolled back, leaving the
     // Reference dangling.
    Reference ref () const;
     // Gets a reference to the value without automatically loading.  If the
     // resource is RS::Unloaded, returns an empty Reference.
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

 // While one of these is active, resource operations will have transactional
 // semantics; that is, the results of the operations won't be finalized until
 // the ResourceTransaction is destructed, at which point either all of the
 // operations will committed, or all of them will be rolled back if an
 // exception was thrown.
 //
 // It is not recommended to modify resource data while one of these is active.
using ResourceTransaction = Transaction<Resource>;

 // Loads a resource into the current purpose.  Does nothing if the resource is
 // not RS::Unloaded.  Throws if the file doesn't exist on disk or can't be opened.
void load (Resource);
 // Load multiple resources.  If an error is thrown, none of the resources will
 // be loaded.
inline void load (Slice<Resource> rs) {
    ResourceTransaction tr;
    for (auto& r : rs) load(r);
}

 // Saves a loaded resource to disk.  Throws if the resource is not RS::Loaded.  May
 // overwrite an existing file on disk.
void save (Resource);
 // Save multiple resources.  If an error is thrown, none of the resources will
 // be saved.
inline void save (Slice<Resource> rs) {
    ResourceTransaction tr;
    for (auto& r : rs) save(r);
}

 // Clears the value of the resource and sets its state to RS::Unloaded.  Does
 // nothing if the resource is RS::Unloaded, and throws if it is LOADING.  Scans all
 // other RS::Loaded resources to make sure none of them are referencing this
 // resource, and if any are, this call will throw UnloadWouldBreak and the
 // resource will not be unloaded.
void unload (Resource);
 // Unload multiple resources simultaneously.  If multiple resources have a
 // reference cycle between them, they must be unloaded simultaneously.
void unload (Slice<Resource>);
inline void unload (Resource r) { unload(Slice<Resource>(&r, 1)); }

 // Immediately unloads the file without scanning for references to it.  This is
 // faster, but if there are any references to data in this resource, they will
 // be left dangling.  This should never fail.
void force_unload (Resource);
 // Unload multiple resources.
inline void force_unload (Slice<Resource> rs) {
    for (auto& r : rs) force_unload(r);
}

 // Reloads a resource that is loaded.  Throws if the resource is not RS::Loaded.
 // Scans all other resources for references to this one and updates them to
 // the new address.  If the reference is no longer valid, this call will throw
 // ReloadWouldBreak, and the resource will be restored to its old value
 // before the call to reload.  It is an error to reload while another resource
 // is being loaded.
void reload (Resource);
 // Reload multiple resources simultaneously.  This may be necessary or simply
 // more efficient if there are reference cycles.
void reload (Slice<Resource>);
inline void reload (Resource r) { reload(Slice<Resource>(&r, 1)); }

///// RESOURCE FILE MANIPULATION (NON-TRANSACTIONAL)

 // Moves old_res's value to new_res.  Does not change the names of any Resource
 // objects, just the mapping from names to values.  Does not affect any files
 // on disk.  Will throw if old_res is not RS::Loaded, or if new_res is not
 // RS::Unloaded.  After renaming, old_res will be RS::Unloaded and new_res will
 // be RS::Loaded.
void rename (Resource old_res, Resource new_res);

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

 // Returns a list of all resources with state != RS::Unloaded.  This includes
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
