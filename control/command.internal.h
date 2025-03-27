#pragma once

#include <utility>
#include "../uni/common.h"
#include "../ayu/reflection/describe-standard.h"

namespace control {
using namespace uni;

///// STATEMENT STORAGE

struct CommandBase;

struct StatementStorageBase {
    const CommandBase* command;
};

template <u32 i, class Par>
struct StatementStorageHead {
    static_assert(!std::is_reference_v<Par>);
    Par arg;
};

template <u32 i, class... Pars>
struct StatementStorageTail;
template <u32 i>
struct StatementStorageTail<i> {
    template <u32 n> static consteval
    auto member_pointer () {
        static_assert((StatementStorageTail*)null, "StatementStorage<...>::member_pointer<n> out of bounds");
    }
};

template <u32 i, class Par, class... Pars>
struct StatementStorageTail<i, Par, Pars...> :
    StatementStorageHead<i, Par>, StatementStorageTail<i+1, Pars...>
{
    using Head = StatementStorageHead<i, Par>;
    using Tail = StatementStorageTail<i+1, Pars...>;

    template <u32 n> static consteval
    auto member_pointer () {
        if constexpr (n == i) return &Head::arg;
        else return Tail::template member_pointer<n>();
    }

    // Make sure to explicitly empty-construct the members.  With =default,
    // unspecified arguments can have indeterminate values.
    StatementStorageTail () : Head(), Tail() { }
    template <class Arg, class... Args>
    StatementStorageTail (Arg&& arg, Args&&... args) :
        Head(std::forward<Arg>(arg)), Tail(std::forward<Args>(args)...)
    { }
};

template <class... Pars>
struct StatementStorage :
    StatementStorageBase, StatementStorageTail<0, Pars...>
{
    StatementStorage () = default;
    template <class Cmd, class... Args>
    StatementStorage (const Cmd& cmd, Args&&... args) :
        StatementStorageBase(&cmd),
        StatementStorageTail<0, Pars...>(std::forward<Args>(args)...)
    { }
};

///// ANALYZING COMMAND FUNCTIONS

template <auto& f, class F = decltype(f)>
struct FunctionInfo;
template <auto& f, class... Pars>
struct FunctionInfo<f, void(&)(Pars...)> {
    using Storage = StatementStorage<std::remove_cvref_t<Pars>...>;
    static constexpr u32 max = sizeof...(Pars);

    template <usize... is>
    static void call (StatementStorageBase* storage) {
         // This ends up unused if the parameter pack is empty
        Storage* st [[maybe_unused]] = static_cast<Storage*>(storage);
        f(st->*(Storage::template member_pointer<is>())...);
    }
    template <usize... is>
    static constexpr auto get_call_mid (std::index_sequence<is...>) {
        return &call<is...>;
    }
    static consteval auto get_call () {
        return get_call_mid(std::index_sequence_for<Pars...>{});
    }
};

//template <auto&, class...>
//struct FunctionInfoLambda;
//template <auto& f, class T, class... Pars>
//struct FunctionInfoLambda<f, void(T::*)(Pars...)> :
//    FunctionInfo<f, void(&)(Pars...)>
//{ };
//
//template <auto& f, class T> requires { T::operator(); }
//struct FunctionInfo<f, T> : FunctionInfoLambda<f, T, &T::operator()> { };

void register_command (const CommandBase*);

 // Based on the std::tuple description, but more efficient because
 // StatementStorage supports member pointers.
template <class... Pars>
struct StatementStorageElems {
    using Storage = StatementStorage<Pars...>;
    using desc = ayu::_AYU_DescribeBase<Storage>;
    template <usize... is> static consteval
    auto make (std::index_sequence<is...>) {
        return desc::elems(
             // Command was written in before_from_tree, so ignore it in
             // from_tree.  TODO: use member here, not base
            desc::elem(
                desc::template base<StatementStorageBase>(), desc::ignored
            ),
             // Make all arguments optional here and check the minimum in
             // Statement's description.
            desc::elem(
                Storage::template member_pointer<is>(), desc::optional
            )...
        );
    }
};

} // namespace control

AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class... Pars),
    AYU_DESCRIBE_TEMPLATE_TYPE(control::StatementStorage<Pars...>),
    desc::computed_name([]()->uni::AnyString{
        if constexpr (sizeof...(Pars) == 0) {
            return "control::StatementStorage<>";
        }
        else {
             // TODO: read elems for desc names?
            static constexpr const ayu::in::Description* const* descs [] = {
                ayu::in::get_indirect_description<Pars>()...
            };
            return ayu::in::make_variadic_name(
                "control::StatementStorage<", descs, sizeof...(Pars)
            );
        }
    }),
    control::StatementStorageElems<Pars...>::make(
        std::index_sequence_for<Pars...>{}
    )
)
