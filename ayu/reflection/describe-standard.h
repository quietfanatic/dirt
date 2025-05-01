// This header provides template ayu descriptions for a few stl types.  The
// corresponding .cpp file provides descriptions for non-template types like
// native integers.  If you want to use things like std::vector in ayu
// descriptions, include this file.

#pragma once
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "../common.h"
#include "../traversal/compound.h"
#include "../traversal/to-tree.h"
#include "../traversal/from-tree.h"
#include "anyref.h"
#include "describe-base.h"

namespace ayu::in {
    AnyString make_optional_name (Type t) noexcept;
    AnyString make_pointer_name (Type t, int flags) noexcept;
    AnyString make_template_name_1 (StaticString prefix, Type t) noexcept;
    AnyString make_variadic_name (StaticString prefix, const Type* types, u32 len) noexcept;
} // ayu::in

 // std::optional serializes to [] for nullopt and [value] for value.  To make
 // it serialize to (missing from object) for nullopt and value for value, use
 // the collapse_optional flag on the parent object's attr.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::optional<T>),
    desc::computed_name([]{
        return ayu::in::make_optional_name(ayu::Type::For<T>());
    }),
    desc::length(desc::template value_funcs<uni::usize>(
        [](const std::optional<T>& v){ return uni::usize(!!v); },
        [](std::optional<T>& v, uni::usize len){
            if (len > 1) {
                ayu::raise_LengthRejected(
                    ayu::Type::For<std::optional<T>>(), 0, 1, len
                );
            }
            if (len) v.emplace();
            else v.reset();
        }
    )),
    desc::contiguous_elems([](std::optional<T>& v){
        uni::expect(v);
         // For some reason, std::to_address returns a const pointer here, which
         // screws up deserialization (you can't deserialize a const item).
        return ayu::AnyPtr(std::addressof(*v));
    })
)

 // std::unique_ptr behaves like std::optional; an empty array means null, and
 // an array of one element means it contains that element.  This currently does
 // not support polymorphic objects, but it could in the future (with an array
 // of size two).
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::unique_ptr<T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::unique_ptr<", ayu::Type::For<T>()
        );
    }),
    desc::length(desc::template value_funcs<uni::usize>(
        [](const std::unique_ptr<T>& v){ return uni::usize(!!v); },
        [](std::unique_ptr<T>& v, uni::usize len){
            if (len > 1) {
                ayu::raise_LengthRejected(
                    ayu::Type::For<std::unique_ptr<T>>(), 0, 1, len
                );
            }
            if (len) v = std::make_unique<T>();
            else v.reset();
        }
    )),
    desc::contiguous_elems([](std::unique_ptr<T>& v){
        return ayu::AnyPtr(std::to_address(v));
    })
)

 // uni arrays
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(uni::UniqueArray<T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "uni::UniqueArray<", ayu::Type::For<T>()
        );
    }),
    desc::length(desc::template value_methods<
        uni::usize, &uni::UniqueArray<T>::size, &uni::UniqueArray<T>::resize
    >()),
    desc::contiguous_elems([](uni::UniqueArray<T>& v){
        return ayu::AnyPtr(v.data());
    })
)
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(uni::AnyArray<T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "uni::AnyArray<", ayu::Type::For<T>()
        );
    }),
    desc::length(desc::template value_methods<
        uni::usize, &uni::AnyArray<T>::size, &uni::AnyArray<T>::resize
    >()),
    desc::contiguous_elems([](uni::AnyArray<T>& v){
         // Make sure to return mut_data() because data() is const/readonly.
         // This array should not become shared while this pointer is active.
        return ayu::AnyPtr(v.mut_data());
    })
)

 // std::vector
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::vector<T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::vector<", ayu::Type::For<T>()
        );
    }),
    desc::length(desc::template value_methods<
        uni::usize, &std::vector<T>::size, &std::vector<T>::resize
    >()),
    desc::contiguous_elems([](std::vector<T>& v){
        return ayu::AnyPtr(v.data());
    })
)

 // std::unordered_map with strings for keys.  We might add a more general
 // unordered_map description later.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::unordered_map<std::string, T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::unordered_map<std::string, ", ayu::Type::For<T>()
        );
    }),
    desc::keys(desc::template mixed_funcs<uni::AnyArray<uni::AnyString>>(
        [](const std::unordered_map<uni::UniqueString, T>& v){
            uni::UniqueArray<uni::AnyString> r;
            for (auto& p : v) {
                r.emplace_back(p.first);
            }
            return uni::AnyArray(r);
        },
        [](std::unordered_map<std::string, T>& v,
           const uni::AnyArray<uni::AnyString>& ks
        ){
            v.clear();
            for (auto& k : ks) {
                v.emplace(k, T());
            }
        }
    )),
    desc::computed_attrs([](std::unordered_map<std::string, T>& v, const uni::AnyString& k){
        auto iter = v.find(k);
        return iter != v.end()
            ? ayu::AnyRef(&iter->second)
            : ayu::AnyRef();
    })
)

 // std::map with strings for keys.  We might add a more general map description
 // later.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::map<std::string, T>),
    desc::flags(desc::no_refs_to_children | desc::no_refs_from_children),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::map<std::string, ", ayu::Type::For<T>()
        );
    }),
    desc::keys(desc::template mixed_funcs<uni::AnyArray<uni::AnyString>>(
        [](const std::map<std::string, T>& v){
            uni::UniqueArray<uni::AnyString> r;
            for (auto& p : v) {
                r.emplace_back(p.first);
            }
            return uni::AnyArray(r);
        },
        [](std::map<std::string, T>& v,
           const uni::AnyArray<uni::AnyString>& ks
        ){
            v.clear();
            for (auto& k : ks) {
                v.emplace(k, T());
            }
        }
    )),
    desc::computed_attrs([](std::map<std::string, T>& v, const uni::AnyString& k){
        auto iter = v.find(k);
        return iter != v.end()
            ? ayu::AnyRef(&iter->second)
            : ayu::AnyRef();
    })
)

 // std::unordered_set.  This container does not support references either to or
 // from any of its children, because of the way that its structure is
 // determines by its content.  In addition, it requires that elements are
 // move-constructible, since there is no way to construct them in-place.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::unordered_set<T>),
    desc::flags(desc::no_refs_to_children | desc::no_refs_from_children),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::unordered_set<", ayu::Type::For<T>()
        );
    }),
    desc::to_tree([](const std::unordered_set<T>& v){
        uni::UniqueArray<ayu::Tree> a;
        for (auto& m : v) a.emplace_back(ayu::item_to_tree(&m));
        return ayu::Tree(a);
    }),
    desc::from_tree([](std::unordered_set<T>& v, const ayu::Tree& t){
        auto a = uni::Slice<ayu::Tree>(t);
        v.clear();
        for (auto& e : a) {
            T tmp;
            ayu::item_from_tree(&tmp, e);
            auto [iter, inserted] = v.emplace(std::move(tmp));
            if (!inserted) ayu::raise(ayu::e_General, uni::cat(
                "Duplicate element given for ",
                ayu::Type::For<std::unordered_set<T>>().name()
            ));
        }
    })
)

 // std::set.  This container does not support references either to or from any
 // of its children, because of the way that its structure is determines by its
 // content.  In addition, it requires that elements are move-constructible,
 // since there is no way to construct them in-place.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::set<T>),
    desc::flags(desc::no_refs_to_children),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::set<", ayu::Type::For<T>()
        );
    }),
    desc::to_tree([](const std::set<T>& v){
        uni::UniqueArray<ayu::Tree> a;
        for (auto& m : v) a.emplace_back(ayu::item_to_tree(&m));
        return ayu::Tree(a);
    }),
    desc::from_tree([](std::set<T>& v, const ayu::Tree& t){
        auto a = uni::Slice<ayu::Tree>(t);
        v.clear();
        for (auto& e : a) {
            T tmp;
            ayu::item_from_tree(&tmp, e);
            auto [iter, inserted] = v.emplace(std::move(tmp));
            if (!inserted) ayu::raise(ayu::e_General, uni::cat(
                "Duplicate element given for ",
                ayu::Type::For<std::set<T>>().name()
            ));
        }
    })
)

 // Raw pointers
 // TODO: figure out if we need to do something for const T*
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(T*),
    []{
        if constexpr (std::is_same_v<T, void>) {
            return desc::name("void*");
        }
        else if constexpr (std::is_same_v<T, const void>) {
            return desc::name("void const*");
        }
        else if constexpr (std::is_same_v<T, volatile void>) {
            return desc::name("void volatile*");
        }
        else if constexpr (std::is_same_v<T, const volatile void>) {
            return desc::name("void const volatile*");
        }
        else {
            return desc::computed_name([]{
                return ayu::in::make_pointer_name(
                    ayu::Type::For<std::remove_cvref_t<T>>(),
                    std::is_const_v<T> | std::is_volatile_v<T> << 1
                );
            });
        }
    }(),
     // This will probably be faster if we skip the delegation, but let's save
     // that until we know we need it.  Note that when we do that we will have
     // to adjust the breakage scanning in resource.cpp.
    []{
        if constexpr (std::is_void_v<std::remove_cv_t<T>>) {
            return desc::to_tree([](T* const& v){
                return ayu::Tree(uni::usize(v), ayu::TreeFlags::PreferHex);
            });
        }
        else {
            return desc::delegate(desc::template assignable<ayu::AnyRef>());
        }
    }()
)

 // Raw arrays T[n].  I can't believe this works.  WARNING: The type name may be
 // incorrect for multidimensional arrays.  TODO: investigate this.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T, uni::usize n),
    AYU_DESCRIBE_TEMPLATE_TYPE(T[n]),
    desc::computed_name([]{
        return uni::AnyString(uni::cat(
            ayu::Type::For<T>().name(),
            '[', n, ']'
        ));
    }),
    desc::length(desc::template constant<uni::usize>(n)),
    desc::contiguous_elems([](T(& v )[n]){
        return ayu::AnyPtr(&v[0]);
    })
)

 // Special case for char[n], mainly to allow string literals to be passed to
 // ayu::dump without surprising behavior.  Note that the deserialization from
 // string must be given exactly n characters and !WILL NOT NUL-TERMINATE! the
 // char[n].
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(uni::usize n),
    AYU_DESCRIBE_TEMPLATE_TYPE(char[n]),
    desc::computed_name([]{
        return uni::AnyString(uni::cat("char[", n, ']'));
    }),
     // Serialize as a string
    desc::to_tree([](const char(& v )[n]){
        return ayu::Tree(uni::Str(v, n));
    }),
     // Deserialize as either a string or an array
    desc::from_tree([](char(& v )[n], const ayu::Tree& tree){
        if (tree.form == ayu::Form::String) {
            auto s = uni::Str(tree);
            if (s.size() != n) {
                 // This might not be exactly the right error code, since it's
                 // meant for arrays, not strings.
                ayu::raise_LengthRejected(
                    ayu::Type::For<char[n]>(), n, n, s.size()
                );
            }
            for (uni::usize i = 0; i < n; i++) {
                v[i] = s[i];
            }
        }
        else if (tree.form == ayu::Form::Array) {
            auto a = uni::Slice<ayu::Tree>(tree);
            if (a.size() != n) {
                ayu::raise_LengthRejected(
                    ayu::Type::For<char[n]>(), n, n, a.size()
                );
            }
            for (uni::usize i = 0; i < n; i++) {
                v[i] = char(a[i]);
            }
        }
        else {
            ayu::raise_FromTreeFormRejected(
                ayu::Type::For<char[n]>(), tree.form
            );
        }
    }),
     // Allow accessing individual elements like an array
    desc::length(desc::template constant<uni::usize>(n)),
    desc::contiguous_elems([](char(& v )[n]){
        return ayu::AnyPtr(&v[0]);
    })
)

 // std::array
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T, uni::usize n),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::array<T, n>),
    desc::computed_name([]{
        return uni::AnyString(uni::cat(
            "std::array<", ayu::Type::For<T>().name(),
            ", ", n, '>'
        ));
    }),
    desc::length(desc::template constant<uni::usize>(n)),
    desc::contiguous_elems([](std::array<T, n>& v){
        return ayu::AnyPtr(v.data());
    })
)

 // std::pair
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class A, class B),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::pair<A, B>),
    desc::computed_name([]{
        return uni::AnyString(uni::cat(
            "std::pair<", ayu::Type::For<A>().name(),
            ", ", ayu::Type::For<B>().name(), '>'
        ));
    }),
    desc::elems(
        desc::elem(&std::pair<A, B>::first),
        desc::elem(&std::pair<A, B>::second)
    )
)

 // A bit convoluted but hopefully worth it
namespace ayu::in {
     // No recursive templates or extra static tables, just expand the parameter
     // pack right inside of elems(...).  We do need to move this out to an
     // external struct though, to receive an index sequence.
    template <class... Ts>
    struct TupleElems {
        using Tuple = std::tuple<Ts...>;
        using desc = ayu::AYU_DescribeBase<Tuple>;
        template <class T>
        using Getter = T&(*)(Tuple&);
        template <usize... is> static constexpr
        auto make (std::index_sequence<is...>) {
            return desc::elems(
                desc::elem(desc::ref_func(
                    Getter<typename std::tuple_element<is, Tuple>::type>(
                        &std::get<is, Ts...>
                    )
                ))...
            );
        }
    };
}

 // Note that although std::tuple removes references from its members,
 // Ts... is still stuck with references if it has them.  So please
 // std::remove_cvref on the params before instantiating this.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class... Ts),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::tuple<Ts...>),
    desc::computed_name([]()->uni::AnyString{
        static_assert(
            (!std::is_reference_v<Ts> && ...),
            "Cannot instantiate AYU description of a tuple with references as type parameters"
        );
        if constexpr (sizeof...(Ts) == 0) {
            return "std::tuple<>";
        }
        else {
            static constexpr const ayu::Type descs [] = {
                ayu::Type::For_constexpr<Ts>()...
            };
            return ayu::in::make_variadic_name("std::tuple<", descs, sizeof...(Ts));
        }
    }),
    ayu::in::TupleElems<Ts...>::make(
        std::index_sequence_for<Ts...>{}
    )
)
