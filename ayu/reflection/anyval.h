// Represents a dynamically typed object with value semantics.  This is always
// allocated on the heap.  Can only represent types known to ayu.  Can be moved
// but not copied.  There is an empty AnyVal which has no type and no value, but
// unlike Reference, there is no "null" AnyVal which has type and no value.  If
// there is a type there is a value, and vice versa.
//
// AnyVals can either be statically const (e.g. const AnyVal&) or dynamically
// const (having a readonly type).  Arguably the latter is not very useful.
//
// AnyVals cannot be constructed until main() starts (except for the empty
// AnyVal).
#pragma once

#include <type_traits>

#include "../common.internal.h"
#include "pointer.h"
#include "type.h"

namespace ayu {

struct AnyVal {
    Type type;
    Mu* data;

     // The empty value will cause null derefs if you do anything with it.
    constexpr AnyVal () : type(), data(null) { }
     // Create from internal data.  Takes ownership.
    constexpr AnyVal (Type t, Mu*&& d) : type(t), data(d) { d = null; }
     // Default construction
    constexpr explicit AnyVal (Type t) :
        type(t),
        data(t ? t.default_new() : null)
    { }
     // Move construct
    constexpr AnyVal (AnyVal&& o) : type(o.type), data(o.data) {
        o.type = Type();
        o.data = null;
    }
     // Construct with arguments.
    template <class T, class... Args>
    static AnyVal make (Args&&... args) {
        auto type = Type::CppType<T>();
        void* data = type.allocate();
        try {
            new (data) T (std::forward<Args>(args)...);
            return AnyVal(type, reinterpret_cast<Mu*>(data));
        }
        catch (...) {
            type.deallocate(data);
            throw;
        }
    }
     // Move construct from std::unique_ptr
    template <class T>
    AnyVal (std::unique_ptr<T> p) :
        type(Type::CppType<T>()), data((Mu*)p.release())
    { }
     // Move assignment
    constexpr AnyVal& operator = (AnyVal&& o) {
        this->~AnyVal();
        type = o.type;
        data = o.data;
        o.type = Type();
        o.data = null;
        return *this;
    }
     // Destroy
    constexpr ~AnyVal () {
        if (data) type.delete_(data);
    }
     // Check contents.
    constexpr explicit operator bool () const {
        expect(!!type == !!data);
        return !!type;
    }
    constexpr bool empty () const { return !*this; }
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

    Mu& as_known (Type t) {
        expect(type == t);
        return *data;
    }
    template <class T>
    std::remove_cvref_t<T>& as_known () {
        return reinterpret_cast<std::remove_cvref_t<T>&>(
            as_known(Type::CppType<std::remove_cvref_t<T>>())
        );
    }
    template <class T>
    const std::remove_cvref_t<T>& as_known () const {
        return reinterpret_cast<const std::remove_cvref_t<T>&>(
            as_known(Type::CppType<std::remove_cvref_t<T>>())
        );
    }

    template <class T>
    std::unique_ptr<T> to_unique_ptr () && {
        if (empty()) return null;
        auto r = std::unique_ptr<T>(
            (T*)type.cast_to(Type::CppType<T>(), data)
        );
        type = Type();
        data = null;
        return r;
    }
};

} // namespace ayu
