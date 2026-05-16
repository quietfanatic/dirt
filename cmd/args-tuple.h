#pragma once

#include <utility> // std::index_sequence
#include "../uni/common.h"
#include "../ayu/reflection/describe-standard.h"

namespace cmd {
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

 // 
template <i32 min, class... Pars>
struct ArgsTuple :
    ArgsTupleTail<0, Pars...>
{
    static constexpr u32 maximum_parameters = sizeof...(Pars);
    static constexpr u32 minimum_parameters = min < 0 ? maximum_parameters : min;
    static_assert(minimum_parameters <= maximum_parameters);

    ArgsTuple () = default;
    template <class... Args>
    ArgsTuple (Args&&... args) :
        ArgsTupleTail<0, Pars...>(std::forward<Args>(args)...)
    { }

    template <class Ret, class Ctx, auto f, usize... is>
    static Ret handle (Ctx ctx, void* s) {
         // This ends up unused if Pars... is empty
        [[maybe_unused]] auto self = (ArgsTuple*)s;
         // f can return anything convertible to Cmd::Return
        return f(ctx, self->*(ArgsTuple::template member_pointer<is>())...);
    };

    template <class Ret, class Ctx, auto f, usize... is>
    static Ret handle_method (Ctx ctx, void* s) {
         // This ends up unused if Pars... is empty
        [[maybe_unused]] auto self = (ArgsTuple*)s;
         // f can return anything convertible to Cmd::Return
        return (ctx.*f)(self->*(ArgsTuple::template member_pointer<is>())...);
    };
};

template <auto f, i32 min, class F = decltype(f)>
struct ConvertToArgsTupleHandler;
template <auto f, i32 min, class Ret, class Ctx, class... Pars>
struct ConvertToArgsTupleHandler<
    f, min, Ret(*)(Ctx, Pars...)
> {
    using type = ArgsTuple<min, std::remove_cvref_t<Pars>...>;

    template <usize... is>
    static constexpr auto get_handler_mid (std::index_sequence<is...>) {
        return &type::template handle<Ret, Ctx, f, is...>;
    }
    static consteval auto get_handler () {
        return get_handler_mid(std::index_sequence_for<Pars...>{});
    }
};
template <auto f, i32 min, class Ret, class Inv, class... Pars>
struct ConvertToArgsTupleHandler<
    f, min, Ret(Inv::*)(Pars...)
> {
    using type = ArgsTuple<min, std::remove_cvref_t<Pars>...>;

    template <usize... is>
    static constexpr auto get_handler_mid (std::index_sequence<is...>) {
        return &type::template handle_method<Ret, Inv&, f, is...>;
    }
    static consteval auto get_handler () {
        return get_handler_mid(std::index_sequence_for<Pars...>{});
    }
};
template <auto f, i32 min, class Ret, class Inv, class... Pars>
struct ConvertToArgsTupleHandler<
    f, min, Ret(Inv::*)(Pars...)const
> {
    using type = ArgsTuple<min, std::remove_cvref_t<Pars>...>;

    template <usize... is>
    static constexpr auto get_handler_mid (std::index_sequence<is...>) {
        return &type::template handle_method<Ret, const Inv&, f, is...>;
    }
    static consteval auto get_handler () {
        return get_handler_mid(std::index_sequence_for<Pars...>{});
    }
};

template <class Ret, class Ctx, auto f, class Args>
Ret collapse_handle (Ctx ctx, void* args) {
    return f(ctx, *(Args*)args);
}

template <class Ret, class Ctx, auto f, class Args>
Ret collapse_handle_method (Ctx ctx, void* args) {
    return (ctx.*f)(*(Args*)args);
}

 // This technically doesn't belong here
template <auto f, class F = decltype(f)>
struct ConvertToCollapseHandler;
template <auto f, class Ret, class Ctx, class Args>
struct ConvertToCollapseHandler<
    f, Ret(*)(Ctx, Args)
> {
    using type = std::remove_cvref_t<Args>;
    static constexpr auto get_handler () {
        return &collapse_handle<Ret, Ctx, f, type>;
    }
};
template <auto f, class Ret, class Ctx, class Args>
struct ConvertToCollapseHandler<
    f, Ret(Ctx::*)(Args)
> {
    using type = std::remove_cvref_t<Args>;
    static constexpr auto get_handler () {
        return &collapse_handle_method<Ret, Ctx, f, type>;
    }
};
 // For more than one parameter we still need to use ArgsTuple
template <auto f, class Ret, class Ctx, class Arg, class... Args>
struct ConvertToCollapseHandler<
    f, Ret(*)(Ctx, Arg, Args...)
> : ConvertToArgsTupleHandler<f, -1, Ret(*)(Ctx, Arg, Args...)> { };
template <auto f, class Ret, class Ctx, class Arg, class... Args>
struct ConvertToCollapseHandler<
    f, Ret(Ctx::*)(Arg, Args...)
> : ConvertToArgsTupleHandler<f, -1, Ret(Ctx::*)(Arg, Args...)> { };

 // Based on the std::tuple description, but more efficient because
 // ArgsTuple supports member pointers.
template <i32 min, class... Pars>
struct ArgsTupleElems {
    using Args = ArgsTuple<min, Pars...>;
    using desc = ayu::AYU_DescribeBase<Args>;

    template <usize... is> static consteval
    auto make (std::index_sequence<is...>) {
        return desc::elems(
            desc::elem(
                Args::template member_pointer<is>(),
                min < 0
                    ? is == sizeof...(is) - 1
                         // Have to use include instead of collapse, otherwise
                         // the child element can't be properly linked to.
                        ? desc::include
                        : decltype(desc::include){}
                    : i32(is) >= min
                        ? desc::optional
                        : decltype(desc::optional){}
            )...
        );
    }
};

[[gnu::noclone]] NOINLINE inline
SharedString make_ArgsTuple_name (i32 min, StaticArray<ayu::Type> types) {
    expect(types);
    return cat(
        "cmd::ArgsTuple<", min, ", ",
        Caterator(", ", types.size(), [types](u32 i){
            return expect(types[i].name());
        }), '>'
    );
}

} // cmd

AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(uni::i32 min, class... Pars),
    AYU_DESCRIBE_TEMPLATE_TYPE(cmd::ArgsTuple<min, Pars...>),
    []{
        if constexpr (sizeof...(Pars) == 0) {
            return desc::name("cmd::ArgsTuple<0>");
        }
        else {
            return desc::computed_name([]()->uni::SharedString{
                 // TODO: read elems for desc names?
                static constexpr const ayu::Type types [] = {
                    ayu::Type::constexpr_of<Pars>()...
                };
                return cmd::make_ArgsTuple_name(
                    min, uni::StaticArray<ayu::Type>(types)
                );
            });
        }
    }(),
    cmd::ArgsTupleElems<min, Pars...>::make(
        std::index_sequence_for<Pars...>{}
    )
)
