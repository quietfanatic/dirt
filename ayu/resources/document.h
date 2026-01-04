#pragma once
#include "../common.h"
#include "../reflection/type.h"
#include "document.internal.h"

namespace ayu {

// This is an owning container for dynamic values with optional names, intended
// to be the top-level item of a resource.  You can think of it like an
// unordered_map<AnyVal>, except that order is preserved and it's much more
// efficient for most use cases.
//
// Anonymous items are assigned a sequential integer id.  This can be referred
// to like it's a name by putting _ before the decimal representation of the
// number, like "_36".  These pseudonyms are stored as integers, not strings, so
// anonymous items are cheaper than named items.  Other names starting with _
// are forbidden.
//
// Items are stored in an intrustive linked list, so unnamed item emplacement
// and erasure are fast (a single allocation/deallocation).  Finding items by
// name or ID is usually O(n), but the last lookup is cached so that finding the
// next item in order is O(1).  Named item emplacement is O(n) to preserve name
// uniqueness.
//
// Item types cannot have an alignment larger than 8.  This restriction is
// inherited from lilac.

struct Document : in::DocumentData {

    Document () { }

     // Documents are movable but not copyable.
    Document (Document&& o) : DocumentData(move(o)) { }
    Document& operator= (Document&& o) {
        this->~Document();
        new (this) Document(move(o));
        return *this;
    }

     // Emplace a new anonymous item into this document.  This should be
     // basically as fast as an ordinary allocation with operator new.
    template <Describable T, class... Args>
    T* new_ (Args&&... args) {
        void* p = allocate(Type::For<T>());
        try {
            return new (p) T {std::forward<Args...>(args...)};
        }
        catch (...) {
            deallocate(p);
            throw;
        }
    }

     // Emplace a new named item into this document.  This may scan all the
     // items in the document to enfore name uniqueness.
    template <Describable T, class... Args>
    T* new_named (AnyString name, Args&&... args) {
        void* p = allocate_named(Type::For<T>(), move(name));
        try {
            return new (p) T (std::forward<Args...>(args...));
        }
        catch (...) {
            deallocate(p);
            throw;
        }
    }

     // Delete an item in this document.  In debug mode, verifies that the given
     // object actually belongs to this Document and that its type is actually
     // T.  In release mode, should be basically as fast as a plain operator
     // delete.
    template <Describable T>
    void delete_ (T* p) {
        delete_(Type::For<T>(), (Mu*)p);
    }

     // Return pointer to the item with the given name.  Can find anonymous
     // items if you pass a decimal integer prefixed with _.  Returns null if
     // not found or if the name is invalid (empty or starts with _ but is not
     // an anonymous id)
    AnyPtr find_with_name (Str) const;

     // Allocates space for this type but does not construct it.
    void* allocate (Type) noexcept;
    void* allocate_named (Type, AnyString);

     // Destructs and deallocates.
    void delete_ (Type, Mu*) noexcept;
     // Deletes by name and throws if not found.
    void delete_named (Str);

     // Deallocates without destructing.
    void deallocate (void* p) noexcept;
};

 // Tried to create a document item with an invalid name (empty or starting
 // with a _).
constexpr ErrorCode e_DocumentItemNameInvalid = "ayu::e_DocumentItemNameInvalid";
 // Tried to create a document item with a name that's already in use in
 // this document.
constexpr ErrorCode e_DocumentItemNameDuplicate = "ayu::e_DocumentItemNameDuplicate";
 // Tried to delete a document item by name, but the given name isn't in this
 // document.
constexpr ErrorCode e_DocumentItemNotFound = "ayu::e_DocumentItemNotFound";

} // namespace ayu
