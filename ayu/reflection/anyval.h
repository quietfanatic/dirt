// Represents a dynamically typed object with value semantics.  This is always
// allocated on the heap.  Can only represent types known to ayu.  Can be moved
// but not copied.  There is an empty AnyVal which has no type and no value, but
// unlike AnyRef, there is no "null" AnyVal which has type and no value.  If
// there is a type there is a value, and vice versa.
//
// AnyVals can be statically const (const AnyVal&) but not dynamically const
// (readonly) like AnyPtr.
//
// AnyVals cannot be constructed until main() starts (except for the empty
// AnyVal).

#pragma once
#include <type_traits>
#include "../common.internal.h"
#include "anyptr.h"
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
        data(t ? dynamic_default_new(t) : null)
    { }
     // Move construct
    constexpr AnyVal (AnyVal&& o) : type(o.type), data(o.data) {
        o.type = Type();
        o.data = null;
    }
     // Construct with arguments.
    template <Describable T, class... Args>
    static AnyVal make (Args&&... args) {
        auto type = Type::For<T>();
        void* data = dynamic_allocate(type);
        try {
            new (data) T (std::forward<Args>(args)...);
            return AnyVal(type, reinterpret_cast<Mu*>(data));
        }
        catch (...) {
            dynamic_deallocate(type, data);
            throw;
        }
    }
     // Move construct from std::unique_ptr
    template <Describable T>
    AnyVal (std::unique_ptr<T> p) :
        type(Type::For<T>()), data((Mu*)p.release())
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
        if (data) dynamic_delete(type, data);
    }
     // Check contents.
    constexpr explicit operator bool () const {
        expect(!!type == !!data);
        return !!type;
    }
    constexpr bool empty () const { return !*this; }
     // Get AnyPtr to the value
    constexpr AnyPtr ptr () { return AnyPtr(type, data); }
    AnyPtr readonly_ptr () const { return AnyPtr(type, data, true); }
     // Runtime casting
    Mu& as (Type t) {
        return *dynamic_upcast(type, t, data);
    }
    template <class T>
    std::remove_cvref_t<T>& as () {
        return reinterpret_cast<std::remove_cvref_t<T>&>(
            as(Type::For<std::remove_cvref_t<T>>())
        );
    }
    template <class T>
    const std::remove_cvref_t<T>& as () const {
        return reinterpret_cast<const std::remove_cvref_t<T>&>(
            as(Type::For<std::remove_cvref_t<T>>())
        );
    }

    Mu& as_known (Type t) {
        expect(type == t);
        return *data;
    }
    template <class T>
    std::remove_cvref_t<T>& as_known () {
        return reinterpret_cast<std::remove_cvref_t<T>&>(
            as_known(Type::For<std::remove_cvref_t<T>>())
        );
    }
    template <class T>
    const std::remove_cvref_t<T>& as_known () const {
        return reinterpret_cast<const std::remove_cvref_t<T>&>(
            as_known(Type::For<std::remove_cvref_t<T>>())
        );
    }

    template <Describable T>
    std::unique_ptr<T> to_unique_ptr () && {
        if (empty()) return null;
        auto r = std::unique_ptr<T>(
            (T*)dynamic_upcast(type, Type::For<T>(), data)
        );
        type = Type();
        data = null;
        return r;
    }
};

} // namespace ayu
