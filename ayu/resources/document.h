#pragma once

#include "../common.h"
#include "../reflection/type.h"

namespace ayu {

// This is a type storing dynamic values with optional names, intended to be the
// top-level item of a file.  Has fast insertion of newly-created unnamed items
// (usually one allocation including the new item).
//
// Keys starting with _ are reserved.
struct Document {
    in::DocumentData* data;

    Document () noexcept;
     // Deletes all items
    ~Document ();
    Document (const Document&) = delete;
    Document (Document&& o) : data(o.data) { o.data = null; }
    Document& operator= (Document&& o) {
        this->~Document();
        data = o.data; o.data = null;
        return *this;
    }

    template <class T, class... Args>
    T* new_ (Args&&... args) {
        void* p = allocate(Type::CppType<T>());
        try {
            return new (p) T {std::forward<Args...>(args...)};
        }
        catch (...) {
            deallocate(p);
            throw;
        }
    }

     // This may be linear over the number of items in the document.
    template <class T, class... Args>
    T* new_named (AnyString name, Args&&... args) {
        void* p = allocate_named(Type::CppType<T>(), move(name));
        try {
            return new (p) T (std::forward<Args...>(args...));
        }
        catch (...) {
            deallocate(p);
            throw;
        }
    }

     // In debug mode, verifies that the given object actually belongs to this
     // Document and that its type is actually T.
    template <class T>
    void delete_ (T* p) {
        delete_(Type::CppType<T>(), (Mu*)p);
    }

    void* allocate (Type) noexcept;
    void* allocate_named (Type, AnyString);
    void delete_ (Type, Mu*) noexcept;
    void delete_named (Str);

    void deallocate (void* p) noexcept;
};

 // Tried to create a document item with an invalid name (empty or starting
 // with a _).
constexpr ErrorCode e_DocumentItemNameInvalid = "ayu::e_DocumentItemNameInvalid";
 // Tried to create a document item with a name that's already in use in
 // this document.
constexpr ErrorCode e_DocumentItemNameDuplicate = "ayu::e_DocumentItemNameDuplicate";
 // Tried to delete a document item by name, but the given name isn't in this
 // document.  TODO: Replace delete_named with find_by_name
constexpr ErrorCode e_DocumentItemNotFound = "ayu::e_DocumentItemNotFound";

} // namespace ayu
