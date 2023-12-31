// Represents a dynamically typed object with value semantics.  This is always
// allocated on the heap.  Can only represent types known to ayu.  Can be moved
// but not copied.  There is an empty Dynamic which has no type and no value,
// but unlike Reference, there is no "null" Dynamic which has type and no value.
// If there is a type there is a value, and vice versa.
//
// Dynamics can either be statically const (e.g. const Dynamic&) or dynamically
// const (having a readonly type).
//
// Dynamics cannot be constructed until main() starts (except for the empty
// Dynamic).
#pragma once

#include <type_traits>

#include "../common.internal.h"
#include "pointer.h"
#include "type.h"

namespace ayu {

struct Dynamic {
    const Type type;
    Mu* const data;

     // The empty value will cause null derefs if you do anything with it.
    constexpr Dynamic () : type(), data(null) { }
     // Create from internal data.  Takes ownership.
    Dynamic (Type t, Mu*&& d) : type(t), data(d) { d = null; }
     // Default construction
    explicit Dynamic (Type t) :
        type(t),
        data(t ? t.default_new() : null)
    { }
     // Move construct
    Dynamic (Dynamic&& o) : type(o.type), data(o.data) {
        const_cast<Type&>(o.type) = Type();
        const_cast<Mu*&>(o.data) = null;
    }
     // Construct by moving an arbitrary type in
    template <class T> requires (
        !std::is_base_of_v<Dynamic, T>
        && !std::is_base_of_v<Type, T>
        && !std::is_reference_v<T>
    )
    Dynamic (T&& v) : type(Type::CppType<T>()), data(reinterpret_cast<Mu*>(type.allocate())) {
        try {
            new (data) T (move(v));
        }
        catch (...) {
            type.deallocate(data);
            throw;
        }
    }
     // Construct with arguments.
    template <class T, class... Args>
    static Dynamic make (Args&&... args) {
        auto type = Type::CppType<T>();
        void* data = type.allocate();
        try {
            new (data) T (std::forward<Args>(args)...);
            return Dynamic(type, reinterpret_cast<Mu*>(data));
        }
        catch (...) {
            type.deallocate(data);
            throw;
        }
    }
     // Move assignment
    Dynamic& operator = (Dynamic&& o) {
        this->~Dynamic();
        new (this) Dynamic (move(o));
        return *this;
    }
     // Destroy
    ~Dynamic () {
        if (data) type.delete_(data);
    }
     // Check contents.  No coercion to bool because that would be confusing.
    bool has_value () const {
        expect(!!type == !!data);
        return !!type;
    }
    bool empty () const {
        expect(!!type == !!data);
        return !type;
    }
     // Get Pointer to the value
    Pointer ptr () { return Pointer(type, data); }
    Pointer readonly_ptr () const { return Pointer(type.add_readonly(), data); }
     // Runtime casting
    Mu& as (Type t) {
        return *type.cast_to(t, data);
    }
    template <class T>
    std::remove_cvref_t<T>& as () {
        return reinterpret_cast<std::remove_cvref_t<T>&>(
            as(Type::CppType<std::remove_cvref_t<T>>())
        );
    }
    template <class T>
    const std::remove_cvref_t<T>& as () const {
        return reinterpret_cast<const std::remove_cvref_t<T>&>(
            as(Type::CppType<std::remove_cvref_t<T>>())
        );
    }
     // Copying accessor
    template <class T>
    std::remove_cvref_t<T> get () const {
        return as<std::remove_cvref_t<T>>();
    }
     // Explicit coercion
    template <class T> requires (
        !std::is_base_of_v<Dynamic, T>
        && !std::is_base_of_v<Type, T>
        && !std::is_reference_v<T>
    )
    explicit operator T& () {
        return as<T>();
    }
    template <class T> requires (
        !std::is_base_of_v<Dynamic, T>
        && !std::is_base_of_v<Type, T>
        && !std::is_reference_v<T>
    )
    explicit operator const T& () const {
        return as<T>();
    }
};

} // namespace ayu
