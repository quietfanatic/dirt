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
    Type type;
    Mu* data;

     // The empty value will cause null derefs if you do anything with it.
    constexpr Dynamic () : type(), data(null) { }
     // Create from internal data.  Takes ownership.
    constexpr Dynamic (Type t, Mu*&& d) : type(t), data(d) { d = null; }
     // Default construction
    constexpr explicit Dynamic (Type t) :
        type(t),
        data(t ? t.default_new() : null)
    { }
     // Move construct
    constexpr Dynamic (Dynamic&& o) : type(o.type), data(o.data) {
        o.type = Type();
        o.data = null;
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
    constexpr Dynamic& operator = (Dynamic&& o) {
        this->~Dynamic();
        type = o.type;
        data = o.data;
        o.type = Type();
        o.data = null;
        return *this;
    }
     // Destroy
    constexpr ~Dynamic () {
        if (data) type.delete_(data);
    }
     // Check contents.  No coercion to bool because that would be confusing.
    constexpr bool has_value () const {
        expect(!!type == !!data);
        return !!type;
    }
    constexpr bool empty () const {
        expect(!!type == !!data);
        return !type;
    }
     // Get Pointer to the value
    constexpr Pointer ptr () { return Pointer(type, data); }
    constexpr Pointer readonly_ptr () const { return Pointer(type.add_readonly(), data); }
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
};

} // namespace ayu
