#pragma once
#include <utility>
#include "common.h"

namespace uni {

 // A type-erased generic function that's heap-allocated.  Similar to
 // std::move_only_function
template <class> struct UniqueFunction;
template <class Ret, class... Args>
struct UniqueFunction<Ret(Args...)> {
     ///// INTERNAL
    struct Base {
         // Don't use builtin virtual functions, they cause binary bloat
         // especially in template classes.  Also we're putting both of these
         // function pointers directly in the object, since it isn't worth
         // having a separate vtable for just two functions.
        Ret (* call_p )(Base*, Args...);
        void (* delete_p )(Base*);

        template <class F> static
        Ret do_call (Base* self, Args... args) {
            auto imp = static_cast<Imp<F>*>(self);
            return imp->f(std::forward<Args>(args)...);
        }
        template <class F> static
        void do_delete (Base* self) {
            delete static_cast<Imp<F>*>(self);
        }
    };
    template <class F> struct Imp : Base { F f; };

    Base* imp;

    ///// INTERFACE
     // Construct by moving any callable object
    template <class F> requires (
         // Don't accidentally nest UniqueFunctions
        !std::is_base_of_v<std::remove_cvref_t<F>, UniqueFunction> &&
        std::is_convertible_v<std::invoke_result_t<F, Args...>, Ret>
    ) ALWAYS_INLINE
    UniqueFunction (F&& f) :
        imp(new Imp<std::remove_cvref_t<F>>{
            Base{
                &Base::template do_call<std::remove_cvref_t<F>>,
                &Base::template do_delete<std::remove_cvref_t<F>>,
            },
            std::forward<F>(f)
        })
    { }

     // Move construct and assign
    ALWAYS_INLINE
    UniqueFunction (UniqueFunction&& o) : imp(o.imp) { o.imp = null; }
    ALWAYS_INLINE
    UniqueFunction& operator= (UniqueFunction&& o) {
        if (imp) imp->delete_p(imp);
        imp = o.imp; o.imp = null;
        return *this;
    }
     // Delete copy construct and assign
    UniqueFunction (const UniqueFunction&) = delete;
    UniqueFunction& operator= (const UniqueFunction&) = delete;

     // For some reason C++ isn't able to automatically combine the converting
     // constructor with the move assignment.
    template <class F> ALWAYS_INLINE
    UniqueFunction& operator= (F&& f) {
        if (imp) imp->delete_p(imp);
        imp = new Imp<std::remove_cvref_t<F>>{
            Base{
                &Base::template do_call<std::remove_cvref_t<F>>,
                &Base::template do_delete<std::remove_cvref_t<F>>,
            },
            std::forward<F>(f)
        };
        return *this;
    }

     // Construct empty UniqueFunction
    ALWAYS_INLINE constexpr
    UniqueFunction (std::nullptr_t n = null) : imp(n) { }

     // Test for emptiness
    ALWAYS_INLINE constexpr explicit
    operator bool () const { return !!imp; }

     // Call
    ALWAYS_INLINE
    Ret operator() (Args... args) {
        return imp->call_p(imp, std::forward<Args>(args)...);
    }

     // Destroy
    ALWAYS_INLINE constexpr
    ~UniqueFunction () { if (imp) imp->delete_p(imp); }
};

} // uni
