#pragma once

#include <utility> // std::index_sequence
#include "../uni/common.h"
#include "../ayu/reflection/describe-standard.h"

namespace control {
using namespace uni;

///// ARGS TUPLE
 // This is like std::tuple except:
 //   - It captures a "minimum required parameters" value.
 //   - It allows generating member pointers.

template <u32 i, class Par>
struct ArgsTupleHead {
    static_assert(!std::is_reference_v<Par>);
    Par arg;
};

template <u32 i, class... Pars>
struct ArgsTupleTail;
template <u32 i>
struct ArgsTupleTail<i> {
    template <u32 n> static consteval
    auto member_pointer () {
        static_assert((ArgsTupleTail*)null, "StatementStorage<...>::member_pointer<n> out of bounds");
    }
};

template <u32 i, class Par, class... Pars>
struct ArgsTupleTail<i, Par, Pars...> :
    ArgsTupleHead<i, Par>, ArgsTupleTail<i+1, Pars...>
{
    using Head = ArgsTupleHead<i, Par>;
    using Tail = ArgsTupleTail<i+1, Pars...>;

    template <u32 n> static consteval
    auto member_pointer () {
        if constexpr (n == i) return &Head::arg;
        else return Tail::template member_pointer<n>();
    }

    // Make sure to explicitly empty-construct the members.  With =default,
    // unspecified arguments can have indeterminate values.
    ArgsTupleTail () : Head(), Tail() { }
    template <class Arg, class... Args>
    ArgsTupleTail (Arg&& arg, Args&&... args) :
        Head(std::forward<Arg>(arg)), Tail(std::forward<Args>(args)...)
    { }
};

template <u32 min, class... Pars>
struct ArgsTuple :
    ArgsTupleTail<0, Pars...>
{
    static constexpr u32 minimum_parameters = min;
    static constexpr u32 maximum_parameters = sizeof...(Pars);
    static_assert(minimum_parameters <= maximum_parameters);

    ArgsTuple () = default;
    template <class... Args>
    ArgsTuple (Args&&... args) :
        ArgsTupleTail<0, Pars...>(std::forward<Args>(args)...)
    { }

    template <class Cmd, auto f, usize... is>
    static Cmd::Return handle (Cmd::Context ctx, void* s) {
         // This ends up unused if Pars... is empty
        [[maybe_unused]] auto self = (ArgsTuple*)s;
         // f can return anything convertible to Cmd::Return
        return f(ctx, self->*(ArgsTuple::template member_pointer<is>())...);
    };

    template <class Cmd, auto f, usize... is>
    static Cmd::Return handle_method (Cmd::Context ctx, void* s) {
         // This ends up unused if Pars... is empty
        [[maybe_unused]] auto self = (ArgsTuple*)s;
         // f can return anything convertible to Cmd::Return
        return (ctx.*f)(self->*(ArgsTuple::template member_pointer<is>())...);
    };
};

template <class Cmd, auto f, u32 min, class F = decltype(f)>
struct ConvertToArgsTupleHandler;
template <class Cmd, u32 min, auto f, class Ret, class Ctx, class... Pars>
struct ConvertToArgsTupleHandler<
    Cmd, f, min, Ret(*)(Ctx, Pars...)
> {
    using type = ArgsTuple<min, std::remove_cvref_t<Pars>...>;

    template <usize... is>
    static constexpr auto get_handler_mid (std::index_sequence<is...>) {
        return &type::template handle<Cmd, f, is...>;
    }
    static consteval auto get_handler () {
        return get_handler_mid(std::index_sequence_for<Pars...>{});
    }
};
template <class Cmd, u32 min, auto f, class Ret, class Ctx, class... Pars>
struct ConvertToArgsTupleHandler<
    Cmd, f, min, Ret(Ctx::*)(Pars...)
> {
    using type = ArgsTuple<min, std::remove_cvref_t<Pars>...>;

    template <usize... is>
    static constexpr auto get_handler_mid (std::index_sequence<is...>) {
        return &type::template handle_method<Cmd, f, is...>;
    }
    static consteval auto get_handler () {
        return get_handler_mid(std::index_sequence_for<Pars...>{});
    }
};

template <class Cmd, auto f, class Args>
typename Cmd::Return collapsed_handle (typename Cmd::Context ctx, void* args) {
    return f(ctx, *(Args*)args);
}

template <class Cmd, auto f, class Args>
typename Cmd::Return collapsed_handle_method (typename Cmd::Context ctx, void* args) {
    return (ctx.*f)(*(Args*)args);
}

 // This technically doesn't belong here
template <class Cmd, auto f, class F = decltype(f)>
struct ConvertToCollapsedHandler;
template <class Cmd, auto f, class Ret, class Ctx, class Args>
struct ConvertToCollapsedHandler<
    Cmd, f, Ret(*)(Ctx, Args)
> {
    using type = std::remove_cvref_t<Args>;
    static constexpr auto get_handler () {
        return &collapsed_handle<Cmd, f, type>;
    }
};
template <class Cmd, auto f, class Ret, class Ctx, class Args>
struct ConvertToCollapsedHandler<
    Cmd, f, Ret(Ctx::*)(Args)
> {
    using type = std::remove_cvref_t<Args>;
    static constexpr auto get_handler () {
        return &collapsed_handle_method<Cmd, f, type>;
    }
};

 // Based on the std::tuple description, but more efficient because
 // ArgsTuple supports member pointers.
template <u32 min, class... Pars>
struct ArgsTupleElems {
    using Args = ArgsTuple<min, Pars...>;
    using desc = ayu::AYU_DescribeBase<Args>;

    template <usize... is> static consteval
    auto make (std::index_sequence<is...>) {
        return desc::elems(
            desc::elem(
                Args::template member_pointer<is>(),
                is >= min ? desc::optional : decltype(desc::optional){}
            )...
        );
    }
};

[[gnu::noclone]] NOINLINE inline
AnyString make_ArgsTuple_name (u32 min, StaticArray<ayu::Type> types) {
    expect(types);
    return cat(
        "control::ArgsTuple<", min, ", ",
        Caterator(", ", types.size(), [types](u32 i){
            return expect(types[i].name());
        }), '>'
    );
}

} // namespace control

AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(uni::u32 min, class... Pars),
    AYU_DESCRIBE_TEMPLATE_TYPE(control::ArgsTuple<min, Pars...>),
    []{
        if constexpr (sizeof...(Pars) == 0) {
            return desc::name("control::ArgsTuple<0>");
        }
        else {
            return desc::computed_name([]()->uni::AnyString{
                 // TODO: read elems for desc names?
                static constexpr const ayu::Type types [] = {
                    ayu::Type::For_constexpr<Pars>()...
                };
                return control::make_ArgsTuple_name(
                    min, uni::StaticArray<ayu::Type>(types)
                );
            });
        }
    }(),
    control::ArgsTupleElems<min, Pars...>::make(
        std::index_sequence_for<Pars...>{}
    )
)
