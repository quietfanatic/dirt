 // CallbackRef<Ret(Args...)>
 // A super lightweight callback class with reference semantics (std::function
 // has value semantics and can be copied and moved, so it's way bigger.)

#pragma once

#include <type_traits>

namespace uni {

template <class> struct CallbackRef;
template <class Ret, class... Args>
struct CallbackRef<Ret(Args...)> {
    void* capture;
    Ret(* wrapper )(void*, Args...);
#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
     // GCC allows casting a method pointer to a function pointer, so take
     // advantage of that if we can.  This is not mainly for optimization (the
     // compiler does that pretty well already), it's to clean up the stack
     // for debugging.  With this the stack goes straight from the caller to the
     // callee with no ugly wrapper functions inbetween.  This might not work
     // for virtual operator() but why would you do that.
    template <class F> requires(
        !requires (CallbackRef* p, F f) { p = &f; } &&
        std::is_same_v<std::invoke_result_t<F, Args...>, Ret>
    )
    [[gnu::artificial]] ALWAYS_INLINE
    constexpr CallbackRef (F&& f) :
        capture((void*)&f),
        wrapper((decltype(wrapper))(&std::remove_cvref_t<F>::operator()))
    { }
    template <class F> requires(
        !requires (CallbackRef* p, F f) { p = &f; } &&
        !std::is_same_v<std::invoke_result_t<F, Args...>, Ret> &&
        std::is_convertible_v<std::invoke_result_t<F, Args...>, Ret>
    )
    [[gnu::artificial]] ALWAYS_INLINE
    constexpr CallbackRef (F&& f) :
        capture((void*)&f),
        wrapper([](void* f, Args... args)->Ret{
            return (std::forward<F>(
                *reinterpret_cast<std::remove_reference_t<F>*>(f)
            ))(std::forward<Args>(args)...);
        })
    { }
#pragma GCC diagnostic pop
#else
    template <class F> requires(
         // Don't accidentally override copy constructor!
        !requires (CallbackRef* p, F f) { p = &f; } &&
        std::is_convertible_v<std::invoke_result_t<F, Args...>, Ret>
    )
    [[gnu::artificial]] ALWAYS_INLINE
    constexpr CallbackRef (F&& f) :
        capture((void*)&f),
        wrapper([](void* f, Args... args)->Ret{
            return (std::forward<F>(
                *reinterpret_cast<std::remove_reference_t<F>*>(f)
            ))(std::forward<Args>(args)...);
        })
    { }
#endif

     // If your're only capturing a single thing, this will be more efficient
     // than using a lambda (which may use more stack space).
#pragma GCC diagnostic push
 // Complains about casting function taking reference to function taking void*.
 // These are compatible in every ABI I'm aware of.
#pragma GCC diagnostic ignored "-Wcast-function-type"
    template <class C>
    [[gnu::artificial]] ALWAYS_INLINE
    constexpr CallbackRef (C&& c, std::type_identity_t<Ret(*)(C, Args...)> f) :
        capture((void*)&c),
        wrapper(reinterpret_cast<decltype(wrapper)>(f))
    { }
#pragma GCC diagnostic pop

     // Looks like there's no way to avoid an extra copy of by-value args.
     // (std::function does it too)
    [[gnu::artificial]] ALWAYS_INLINE
    constexpr Ret operator () (Args... args) const {
        return wrapper(capture, std::forward<Args>(args)...);
    }

    template <class Sig> [[gnu::artificial]] ALWAYS_INLINE constexpr
    const CallbackRef<Sig>& reinterpret () const {
        return *(const CallbackRef<Sig>*)this;
    }
};

} // namespace uni
