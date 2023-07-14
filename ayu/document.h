#pragma once

#include "internal/common-internal.h"
#include "type.h"

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

} // namespace ayu
