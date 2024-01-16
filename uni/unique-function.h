#pragma once
#include <memory>

namespace uni {

template <class Ret, class... Args>
struct UniqueFunctionItf {
    Ret (* call )(void*, Args...);
    void (* destroy )(void*);
    ~UniqueFunctionItf () { (*destroy)(this); }
};

template <class F, class Ret, class... Args>
struct UniqueFunctionImp : UniqueFunctionItf<Ret, Args...> {
    F f;
};

template <class> struct UniqueFunction;
template <class Ret, class... Args>
struct UniqueFunction<Ret(Args...)> {
    std::unique_ptr<UniqueFunctionItf<Ret, Args...>> itf;

    template <class F> requires (
        requires (F f, Args... args) {
            f(std::forward<Args>(args)...);
        }
    )
    UniqueFunction (F&& f) :
        itf(new UniqueFunctionImp<F, Ret, Args...>{
            [](void* self, Args... args)->Ret{
                return (
                    (UniqueFunctionImp<F, Ret, Args...>*)self
                )->f(std::forward<Args>(args)...);
            },
            [](void* self){
                (
                    (UniqueFunctionImp<F, Ret, Args...>*)self
                )->f.~F();
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

    constexpr UniqueFunction (std::nullptr_t n = null) : itf(n) { }

    Ret operator() (Args... args) {
        return itf->call(&*itf, std::forward<Args>(args)...);
    }
    constexpr explicit operator bool () const { return !!itf; }
};

} // uni
