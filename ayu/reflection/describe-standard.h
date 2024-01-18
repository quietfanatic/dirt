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
#include "../traversal/from-tree.h"
#include "describe-base.h"
#include "reference.h"

namespace ayu::in {
    NOINLINE inline
    AnyString make_optional_name (Type t) {
        return cat(t.name(), '?');
    }
    NOINLINE inline
    AnyString make_pointer_name (Type t) {
        return cat(t.name(), '*');
    }
    NOINLINE inline 
    AnyString make_template_name_1 (StaticString prefix, Type t) {
        return cat(prefix, t.name(), '>');
    }
    NOINLINE inline
    AnyString make_tuple_name (StaticString* names, usize len) {
        expect(len >= 1);
        return cat(
            "std::tuple<", Caterator(", ", len, [names](usize i){
                return names[i];
            }), '>'
        );
    }
} // ayu::in

 // std::optional serializes to [] for nullopt and [value] for value.  To make
 // it serialize to (missing from object) for nullopt and value for value, use
 // the collapse_optional flag on the parent object's attr.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::optional<T>),
    desc::computed_name([]{
        return ayu::in::make_optional_name(ayu::Type::CppType<T>());
    }),
    desc::length(desc::template value_funcs<uni::usize>(
        [](const std::optional<T>& v){ return uni::usize(!!v); },
        [](std::optional<T>& v, uni::usize len){
            if (len > 1) {
                ayu::raise_LengthRejected(
                    ayu::Type::CppType<std::optional<T>>(), 0, 1, len
                );
            }
            if (len) v.emplace();
            else v.reset();
        }
    )),
    desc::contiguous_elems([](std::optional<T>& v){
        return ayu::Pointer(std::to_address(v));
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
            "std::unique_ptr<", ayu::Type::CppType<T>()
        );
    }),
    desc::length(desc::template value_funcs<uni::usize>(
        [](const std::unique_ptr<T>& v){ return uni::usize(!!v); },
        [](std::unique_ptr<T>& v, uni::usize len){
            if (len > 1) {
                ayu::raise_LengthRejected(
                    ayu::Type::CppType<std::unique_ptr<T>>(), 0, 1, len
                );
            }
            if (len) v = std::make_unique<T>();
            else v.reset();
        }
    )),
    desc::contiguous_elems([](std::unique_ptr<T>& v){
        return ayu::Pointer(std::to_address(v));
    })
)

 // uni arrays
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(uni::UniqueArray<T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "uni::UniqueArray<", ayu::Type::CppType<T>()
        );
    }),
    desc::length(desc::template value_methods<
        uni::usize, &uni::UniqueArray<T>::size, &uni::UniqueArray<T>::resize
    >()),
    desc::contiguous_elems([](uni::UniqueArray<T>& v){
        return ayu::Pointer(v.data());
    })
)
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(uni::AnyArray<T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "uni::AnyArray<", ayu::Type::CppType<T>()
        );
    }),
    desc::length(desc::template value_methods<
        uni::usize, &uni::AnyArray<T>::size, &uni::AnyArray<T>::resize
    >()),
    desc::contiguous_elems([](uni::AnyArray<T>& v){
        return ayu::Pointer(v.data());
    })
)

 // std::vector
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::vector<T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::vector<", ayu::Type::CppType<T>()
        );
    }),
    desc::length(desc::template value_methods<
        uni::usize, &std::vector<T>::size, &std::vector<T>::resize
    >()),
    desc::contiguous_elems([](std::vector<T>& v){
        return ayu::Pointer(v.data());
    })
)

 // std::unordered_map with strings for keys.  We might add a more general
 // unordered_map description later.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::unordered_map<std::string, T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::unordered_map<std::string, ", ayu::Type::CppType<T>()
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
            ? ayu::Reference(&iter->second)
            : ayu::Reference();
    })
)

 // std::map with strings for keys.  We might add a more general map description
 // later.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::map<std::string, T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::map<std::string, ", ayu::Type::CppType<T>()
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
            ? ayu::Reference(&iter->second)
            : ayu::Reference();
    })
)

 // std::unordered_set
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::unordered_set<T>),
    desc::computed_name([]{
        return ayu::in::make_template_name_1(
            "std::unordered_set<", ayu::Type::CppType<T>()
        );
    }),
     // This does an extra copy of all the elements, but it's hard to avoid
     // doing that.
    desc::delegate(desc::template mixed_funcs<uni::UniqueArray<T>>(
        [](const std::unordered_set<T>& v) {
            uni::UniqueArray<T> r; r.reserve(v.size());
            for (const auto& e : v) {
                r.emplace_back_expect_capacity(e);
            }
            return r;
        },
        [](std::unordered_set<T>& v, const uni::UniqueArray<T>& a){
            v.clear();
            for (const auto& e : a) {
                auto [iter, did] = v.emplace(e);
                if (!did) ayu::raise(ayu::e_General, uni::cat(
                    "Duplicate element given for ",
                    ayu::Type::CppType<std::unordered_set<T>>().name()
                ));
            }
        }
    ))
)

 // std::set.  Same as std::unordered_set above, but elements will be serialized
 // in sorted order.
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::set<T>),
    desc::computed_name([]{
        static uni::StaticString r; if (!r) r = ayu::in::make_template_name_1(
            "std::set<", ayu::Type::CppType<T>()
        );
        return r;
    }),
    desc::delegate(desc::template mixed_funcs<uni::UniqueArray<T>>(
        [](const std::set<T>& v) {
            uni::UniqueArray<T> r; r.reserve(v.size());
            for (const auto& e : v) {
                r.emplace_back_expect_capacity(e);
            }
            return r;
        },
        [](std::set<T>& v, const uni::UniqueArray<T>& a){
            v.clear();
            for (const auto& e : a) {
                auto [iter, did] = v.emplace(e);
                if (!did) ayu::raise(ayu::e_General, uni::cat(
                    "Duplicate element given for ",
                    ayu::Type::CppType<std::set<T>>().name()
                ));
            }
        }
    ))
)

 // Raw pointers
 // TODO: figure out if we need to do something for const T*
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T),
    AYU_DESCRIBE_TEMPLATE_TYPE(T*),
    desc::computed_name([]{
        return ayu::in::make_pointer_name(ayu::Type::CppType<T>());
    }),
     // This will probably be faster if we skip the delegate chain, but let's
     // save that until we know we need it.  Note that when we do that we will
     // have to adjust the breakage scanning in resource.cpp.
    desc::delegate(desc::template assignable<ayu::Pointer>())
)

 // Raw arrays T[n] - I can't believe this works
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T, uni::usize n),
    AYU_DESCRIBE_TEMPLATE_TYPE(T[n]),
    desc::computed_name([]{
        return uni::cat(
            ayu::Type::CppType<T>().name(),
            '[', n, ']'
        );
    }),
    desc::length(desc::template constant<uni::usize>(n)),
    desc::contiguous_elems([](T(& v )[n]){
        return ayu::Pointer(&v[0]);
    })
)

 // Special case for char[n], mainly to allow string literals to be passed to
 // ayu::dump without surprising behavior.  Note that the deserialization from
 // string WILL NOT NUL-TERMINATE the char[n].
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(uni::usize n),
    AYU_DESCRIBE_TEMPLATE_TYPE(char[n]),
    desc::computed_name([]{
        return uni::cat("char[", n, ']');
    }),
     // Serialize as a string
    desc::to_tree([](const char(& v )[n]){
        return ayu::Tree(Str(v, n));
    }),
     // Deserialize as either a string or an array
    desc::from_tree([](char(& v )[n], const ayu::Tree& tree){
        if (tree.form == ayu::Form::String) {
            auto s = uni::Str(tree);
            if (s.size() != n) {
                ayu::raise_LengthRejected(
                    ayu::Type::CppType<char[n]>(), n, n, s.size()
                );
            }
            for (uint i = 0; i < n; i++) {
                v[i] = s[i];
            }
        }
        else if (tree.form == ayu::Form::Array) {
            auto a = uni::Slice<ayu::Tree>(tree);
            if (a.size() != n) {
                ayu::raise_LengthRejected(
                    ayu::Type::CppType<char[n]>(), n, n, a.size()
                );
            }
            for (uint i = 0; i < n; i++) {
                v[i] = char(a[i]);
            }
        }
        else {
            ayu::raise_FromTreeFormRejected(
                ayu::Type::CppType<char[n]>(), tree.form
            );
        }
    }),
     // Allow accessing individual elements like an array
    desc::length(desc::template constant<uni::usize>(n)),
    desc::contiguous_elems([](char(& v )[n]){
        return ayu::Pointer(&v[0]);
    })
)

 // std::array
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T, uni::usize n),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::array<T, n>),
    desc::computed_name([]{
        return uni::cat(
            "std::array<" + ayu::Type::CppType<T>().name(),
            ", ", n, '>'
        );
    }),
    desc::length(desc::template constant<uni::usize>(n)),
    desc::contiguous_elems([](std::array<T, n>& v){
        return ayu::Pointer(v.data());
    })
)

 // std::pair
AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class A, class B),
    AYU_DESCRIBE_TEMPLATE_TYPE(std::pair<A, B>),
    desc::computed_name([]{
        return uni::cat(
            "std::pair<", ayu::Type::CppType<A>().name(),
            ", ", ayu::Type::CppType<B>().name(), '>'
        );
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
        using desc = ayu::_AYU_DescribeBase<Tuple>;
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
            uni::StaticString names [] = {
                ayu::Type::CppType<Ts>().name()...
            };
            return ayu::in::make_tuple_name(names, sizeof...(Ts));
        }
    }),
    ayu::in::TupleElems<Ts...>::make(
        std::index_sequence_for<Ts...>{}
    )
)
