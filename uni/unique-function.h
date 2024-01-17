#pragma once
#include <memory>

namespace uni {

 // A type-erased generic function that's heap-allocated.  Similar to
 // std::move_only_function
template <class> struct UniqueFunction;
template <class Ret, class... Args>
struct UniqueFunction<Ret(Args...)> {
    struct Base {
         // Don't use builtin virtual functions, they cause binary bloat
         // especially in template classes.
        Ret (* call )(void*, Args...);
        void (* destroy )(void*);
        ~Base () { (*destroy)(this); }
    };
    template <class F> struct Imp : Base { F f; };

    std::unique_ptr<Base> imp;

    template <class F> requires (
        requires (F f, Args... args) {
            f(std::forward<Args>(args)...);
        }
    )
    UniqueFunction (F&& f) :
        imp(new Imp{
            [](void* self, Args... args)->Ret{
                return ((Imp<F>*)self)->f(std::forward<Args>(args)...);
            },
            [](void* self){
                ((Imp<F>*)self)->f.~F();
            },
            std::forward<F>(f)
        })
    { }

    UniqueFunction (UniqueFunction&&) = default;
    UniqueFunction& operator= (UniqueFunction&&) = default;

    template <class F>
    UniqueFunction& operator= (F&& f) {
        return operator=(UniqueFunction(std::forward<F>(f)));
    }

    constexpr UniqueFunction (std::nullptr_t n = null) : imp(n) { }

    Ret operator() (Args... args) {
        return imp->call(&*imp, std::forward<Args>(args)...);
    }
    constexpr explicit operator bool () const { return !!imp; }
};

} // uni
