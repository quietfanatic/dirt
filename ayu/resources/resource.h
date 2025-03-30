 // A resource represents a top-level named piece of program data.  A
 // resource has:
 //     - a source, which is generally a file on disk
 //     - a name, which is an IRI that identifies the source
 //     - a value, which is an AnyVal
 //     - a state, which is usually RS::Unloaded or RS::Loaded.
 // Resources can be loaded, reloaded, unloaded, and saved.

#pragma once

#include "../../uni/transaction.h"
#include "../common.h"
#include "../data/print.h"
 // TODO: see if we can take out these dependencies
#include "../reflection/anyref.h"
#include "../reflection/anyval.h"

namespace ayu {

///// RESOURCES

 // The possible states a Resource can have.
enum class ResourceState : u8 {
     // This resource is not loaded and has an empty value.
    Unloaded,
     // This resource is currently being loaded.  Its value exists but is
     // currently having item_from_tree run on it.
    Loading,
     // This resource is fully loaded and has a value, though that value may not
     // reflect the source.
    Loaded,
};
using RS = ResourceState;

 // The interface for a Resource, accessed through a SharedResource or
 // ResourceRef.
struct Resource : in::RefCounted {
     // Returns the resource's name as an IRI
    const IRI& name () const noexcept;
     // See enum ResourceState
    ResourceState state () const noexcept;

     // If the resource is RS::Unloaded, automatically loads the resource from
     // disk.  Will throw if the load fails.  If a ResourceTransaction is
     // currently active, the value will be cleared if the ResourceTransaction
     // is rolled back.
    AnyVal& value ();
     // Gets the value without autoloading.  If the state is RS::Unloaded,
     // returns an empty AnyVal and writing to it is Undefined Behavior.  If
     // the state is RS::Loading, returns a value that may not be completely
     // initialized.
    AnyVal& get_value () noexcept;
     // If the resource is RS::Unloaded, sets is state to RS::Loaded without
     // loading from the source, and sets its value.  Throw ResourceStateInvalid
     // if the state is RS::Loading.  Throws ResourceTypeRejected if the
     // ResourceScheme associated with this Resource returns false from
     // accepts_type.
    void set_value (AnyVal&&);

     // Automatically loads and returns a reference to the value, which can be
     // coerced to a pointer.  If a ResourceTransaction is currently active, the
     // value will be cleared if the transaction is rolled back, leaving the
     // reference dangling.
    AnyRef ref () { return value().ptr(); }
     // Gets a reference to the value without automatically loading.  If the
     // resource is RS::Unloaded, returns an empty AnyRef.
    AnyRef get_ref () noexcept { return get_value().ptr(); }

     // Syntax sugar
    auto operator [] (const AnyString& key) { return ref()[key]; }
    auto operator [] (usize index) { return ref()[index]; }

    protected:
    Resource () { }
};

 // A refcounted pointer to a Resource.
struct SharedResource {
    in::RCP<Resource, in::delete_Resource_if_unloaded> data;
    constexpr SharedResource (Resource* d) : data(d) { }

    constexpr SharedResource () { }
     // Refers to the resource with this name, but does not load it yet.
    SharedResource (const IRI& name);
     // Creates the resource already loaded with the given data, without reading
     // from the source.  Will throw ResourceStateInvalid if a resource with
     // this name is already loaded or ResourceValueInvalid if value is empty.
     // This is equivalent to creating the SharedResource and then calling
     // set_value.
    SharedResource (const IRI& name, AnyVal&& value);

    Resource& operator* () const { return *data; }
    Resource* operator-> () const { return &*data; }
    constexpr explicit operator bool () const { return !!data; }
    auto operator [] (const AnyString& key) { return data->ref()[key]; }
    auto operator [] (usize index) { return data->ref()[index]; }
};

 // A non-owning reference to a Resource.
struct ResourceRef {
    Resource* data;
    constexpr ResourceRef (Resource* d) : data(d) { }

    constexpr ResourceRef () { }
    constexpr ResourceRef (const SharedResource& p) : data(p.data.p) { }

    Resource& operator* () const { return *data; }
    Resource* operator-> () const { return data; }
    operator SharedResource () const { return SharedResource(data); }
    constexpr explicit operator bool () const { return !!data; }
    auto operator [] (const AnyString& key) { return data->ref()[key]; }
    auto operator [] (usize index) { return data->ref()[index]; }
};

inline bool operator== (ResourceRef a, ResourceRef b) { return a.data == b.data; }

///// RESOURCE OPERATIONS

 // While one of these is active, resource operations will have transactional
 // semantics.  The operations will mostly run normally, but permanent effects
 // (such as writing files and running destructors) will be delayed until the
 // transaction finishes.  If the transaction finishes due to an exception being
 // thrown which leaves the scope of the ResourceTransaction, those permanent
 // effects will not happen, and instead all affected resources will be restored
 // to how they were before the transaction started.
using ResourceTransaction = Transaction<Resource>;

 // Loads a resource into the current purpose.  Does nothing if the resource is
 // not RS::Unloaded.  Throws if the source doesn't exist or can't be read.
void load (ResourceRef);
 // Load multiple resources.  If an error is thrown, none of the resources will
 // be loaded.
inline void load (Slice<ResourceRef> rs) {
    ResourceTransaction tr;
    for (auto& r : rs) load(r);
}

 // Saves a loaded resource to its source.  Throws if the resource is not
 // RS::Loaded.  May overwrite an existing file.  If called in a
 // ResourceTransaction, no files will actually be written until the transaction
 // succeeds.
void save (ResourceRef, PrintOptions opts = {});
 // Save multiple resources.  If an error is thrown, none of the resources will
 // be saved.
inline void save (Slice<ResourceRef> rs, PrintOptions opts = {}) {
    ResourceTransaction tr;
    for (auto& r : rs) save(r, opts);
}

 // Attempts to unload the given resources and any resources that are not
 // currently reachable.  Essentially, this does a garbage collection on all
 // loaded resources, with special treatment for resources passed as arguments.
 //   - Resources NOT passed as arguments to unload() are considered reachable:
 //        - if there are any SharedResource handles pointing to them, or
 //        - if there is another reachable resource containing a reference to any
 //          item inside them, or
 //        - if there is a tracked variable containing a reference to one of
 //          their items.
 //   - Resources passed as arguments to unload() will ignore SharedResource
 //     handles and are only considered reachable through other resources and
 //     tracked variables.  If a resource passed as an argument is found to be
 //     reachable, a ResourceUnloadWouldBreak exception will be thrown, and no
 //     resources will be unloaded.  Thus if unload() completes successfully,
 //     all arguments will be unloaded, and some non-arguments may be unloaded.
 //
 // Calling unload() without any arguments will do a garbage collection run, but
 // without any hard requirements on what resources to unload.
 //
 // If there are resources that have a reference cycle between them, they must
 // be unloaded at the same time (this can be either from being passed as
 // arguments in the same call or from having no SharedResource handles pointing
 // to them).
 //
 // This operation is fully transactional.  If a recoverable error occurs, no
 // resources will be unloaded.  If called during a ResourceTransaction and the
 // transaction rolls back, all the unloaded resources will be restored to their
 // previous loaded state.
void unload (ResourceRef);
void unload (Slice<ResourceRef> = {});
inline void unload (ResourceRef r) { unload(Slice<ResourceRef>(&r, 1)); }

 // Immediately unloads the resource without checking for reachability.  This is
 // faster, but if there are any references to items in this resource, they will
 // be left dangling.  This can still be rolled back by a ResourceTransaction.
void force_unload (ResourceRef) noexcept;
inline void force_unload (Slice<ResourceRef> rs) noexcept {
    for (auto& r : rs) force_unload(r);
}

 // Reloads a resource.  Throws if the resource is not RS::Loaded.  This does
 // the following steps:
 //   1. Moves the resource's old value to a temporary location.
 //   2. Loads a new value for the resource, as if by calling load().
 //   3. Scans other resources and tracked variables for references to this one
 //     and updates them to point to the new value instead of the old one.  If a
 //     reference would become invalid or cannot be updated, the reload is
 //     cancelled, the resource's old value is restored, and ReloadWouldBreak is
 //     thrown.
 //   4. Destroys the old value.
 //
 // This operation is fully transactional.  If a recoverable exception is thrown
 // or if a surrounding ResourceTransaction rolls back, then the reload will be
 // cancelled, the new value will be deleted, and the old value will be
 // restored.
void reload (ResourceRef);
 // Reload multiple resources simultaneously.  This may be necessary or simply
 // more efficient if there are reference cycles.
void reload (Slice<ResourceRef>);
inline void reload (ResourceRef r) { reload(Slice<ResourceRef>(&r, 1)); }

///// RESOURCE FILE MANIPULATION (NON-TRANSACTIONAL)

 // Moves old_res's value to new_res.  Does not change the names of any Resource
 // objects, just the mapping from names to values.  Does not affect any files
 // on disk.  old_res must be RS::Loaded and new_res must be RS::Unloaded,
 // otherwise an exception will be thrown.  After renaming, old_res will be
 // RS::Unloaded and new_res will be RS::Loaded.
void rename (ResourceRef old_res, ResourceRef new_res);

 // Deletes the source of the resource.  If the source is a file, deletes the
 // file without confirmation.  Does not change the resource's state or value.
 // Does nothing if the source doesn't exist, but throws RemoveSourceFailed
 // if another error occurs (permission denied, etc.)  Calling load on the
 // resource should fail after this.
void remove_source (const IRI&);

 // Returns true if the given resource's file exists on disk.  Does a pretty
 // basic test: it tries to open the file, and returns true if it can or false
 // if it can't.
bool source_exists (const IRI&);

 // Get the filename of the file backing this resource, if it has one.
AnyString resource_filename (const IRI&);

 // Returns a list of all resources with state != RS::Unloaded.  This includes
 // resources that are in the process of being loaded or reloaded.
UniqueArray<SharedResource> loaded_resources () noexcept;

///// TRACKING NON-RESOURCE ITEMS

 // If you want to reference a resource from outside the resource system, you
 // can tell AYU to track on referencing item (usually a pointer, typically a
 // global or a function-level static variable), so that it can:
 //   - update the tracked item if the resource it references is reloaded, and
 //   - not auto-unload any resources that the tracked item references.
 //
 // Resources should not reference a tracked variable.  If they do, they will
 // become unserializable, because the tracked variable does not have an
 // associated Location.
 //
 // You should not call track on an item that is in a resource, because
 // everything in resources is already tracked.  TODO: detect this and
 // debug-assert.
 //
 // You can start tracking an item before you assign to it.  A common idiom is
 // to load a resource and make a static variable point to it like this.
 //
 //     static PageProgram* program = (
 //         ayu::track(program),
 //         ayu::reference_from_iri("res:/liv/page.ayu#program")
 //     );
 //
template <class T>
void track (T& v);

 // Stop tracking a variable.
template <class T>
void untrack (T& v);

///// RESOURCE ERROR CODES

 // Tried to use an invalid IRI as a resource name
constexpr ErrorCode e_ResourceNameInvalid = "ayu::e_ResourceNameInvalid";
 // The ResourceScheme associated with the resource name rejected the name.
constexpr ErrorCode e_ResourceNameRejected = "ayu::e_ResourceNameRejected";
 // The ResourceScheme associated with the resource did not accept the type
 // provided for the resource.  This can happen either while loading from a
 // file, or when setting a resource's value programmatically.
constexpr ErrorCode e_ResourceTypeRejected = "ayu::e_ResourceTypeRejected";
 // Tried to create a resource with an empty AnyVal, or load from a tree that
 // didn't correspond to a valid AnyVal.
constexpr ErrorCode e_ResourceValueInvalid = "ayu::e_ResourceValueInvalid";
 // Tried to perform an operation on a resource, but its state() was not valid
 // for that operation.
constexpr ErrorCode e_ResourceStateInvalid = "ayu::e_ResourceStateInvalid";
 // Tried to unload a resource, but there's still a reference somewhere pointing
 // to an item inside it.
constexpr ErrorCode e_ResourceUnloadWouldBreak = "ayu::e_ResourceUnloadWouldBreak";
 // Tried to reload a resource, but there was a reference pointing to an item
 // inside of it which couldn't be updated for some reason.
constexpr ErrorCode e_ResourceReloadWouldBreak = "ayu::e_ResourceReloadWouldBreak";
 // Failed to delete a resource's source file.
constexpr ErrorCode e_ResourceRemoveSourceFailed = "ayu::e_ResourceRemoveSourceFailed";

///// INTERNAL

namespace in {
    void track_ptr (AnyPtr) noexcept;
    void untrack_ptr (AnyPtr) noexcept;
}
template <class T> void track (T& v) { in::track_ptr(&v); }
template <class T> void untrack (T& v) { in::untrack_ptr(&v); }

} // namespace ayu
