// This module implements static-layout type descriptions, which are
// generated at compile time and accessed at runtime to determine how to
// construct, destroy, and transform objects to and from trees.  The
// descriptions are mostly declarative; the actual serialization code is in
// serialize.cpp.

#pragma once

#include <cstddef> // std::max_align_t
#include <type_traits>

#include "accessors.internal.h"
#include "../data/tree.h"

namespace ayu::in {

// The goal of this module is to allow descriptions to be laid out in memory at
// compile time.  Thanks to recent C++ standards, this is quite possible.
// The caveat is that it is not really possible to generate static pointers to
// static objects.  To get around this, we are representing all "pointers" as
// offsets from the beginning of the description object.

///// SILLY COMPILE-TIME ERROR MESSAGES
} namespace ayu {
template <class T>
static void ERROR_duplicate_descriptors () { }
static void ERROR_element_is_not_a_descriptor_for_this_type () { }
static void ERROR_cannot_have_non_computed_name_after_computed_name () { }
static void ERROR_description_doesnt_have_name_or_computed_name () { }
static void ERROR_attrs_cannot_be_combined_with_keys_and_computed_attrs () { }
static void ERROR_keys_and_computed_attrs_must_be_together () { }
static void ERROR_cannot_have_non_optional_elem_after_optional_elem () { }
static void ERROR_cannot_have_non_invisible_elem_after_invisible_elem () { }
static void ERROR_elems_cannot_be_combined_with_length_and_computed_elems () { }
static void ERROR_cannot_have_length_without_computed_or_contiguous_elems () { }
static void ERROR_cannot_have_both_computed_and_contiguous_elems () { }
static void ERROR_cannot_have_computed_or_contiguous_elems_without_length () { }
} namespace ayu::in {
///// MEMORY LAYOUT

 // We could use [[no_unique_address]] but this is more aggressive at optimizing
 // out empty structs.  The size_t parameter is to prevent multiple CatHeads
 // with the same type from conflicting with one another.
template <size_t, class HeadT>
struct CatHead;
template <size_t i, class HeadT>
    requires (!std::is_empty_v<HeadT>)
struct CatHead<i, HeadT> {
    HeadT head;
    constexpr CatHead (const HeadT& h) : head(h) { }
};
template <size_t i, class HeadT>
    requires (std::is_empty_v<HeadT>)
struct CatHead<i, HeadT> {
     // Ideally this gets discarded by the linker?
    static HeadT head;
    constexpr CatHead (const HeadT&) { }
};
template <size_t i, class HeadT>
    requires (std::is_empty_v<HeadT>)
HeadT CatHead<i, HeadT>::head {};

template <class...>
struct Cat;

template <class HeadT, class... TailTs>
struct Cat<HeadT, TailTs...> :
    CatHead<sizeof...(TailTs), HeadT>, Cat<TailTs...>
{
    using Head = CatHead<sizeof...(TailTs), HeadT>;
    using Tail = Cat<TailTs...>;
    constexpr Cat (const HeadT& h, const TailTs&... t) :
        Head(h),
        Tail(t...)
    { }

    template <class T>
    constexpr T* get (u16 n) {
        if constexpr (std::is_base_of_v<T, HeadT>) {
            if (n == 0) return &this->Head::head;
            else return Tail::template get<T>(n-1);
        }
        else return Tail::template get<T>(n);
    }
    template <class T>
    constexpr const T* get (u16 n) const {
        if constexpr (std::is_base_of_v<T, HeadT>) {
            if (n == 0) return &this->Head::head;
            else return Tail::template get<T>(n-1);
        }
        else return Tail::template get<T>(n);
    }

    template <class F>
    constexpr void for_each (F f) const {
        f(Head::head);
        Tail::template for_each<F>(f);
    }
    template <class F>
    constexpr auto map (F f) const {
        using NewHeadT = decltype(f(Head::head));
        using NewTailT = decltype(Tail::template map<F>(f));
        if constexpr (std::is_void_v<NewHeadT>) {
            return Tail::template map<F>(f);
        }
        else {
            return Cat<NewHeadT, NewTailT>(
                f(Head::head),
                Tail::template map<F>(f)
            );
        }
    }
};

template <>
struct Cat<> {
    constexpr Cat () { }
    template <class T>
    constexpr T* get (u16) {
        return null;
    }
    template <class T>
    constexpr const T* get (u16) const {
        return null;
    }
    template <class F>
    constexpr void for_each (F) const { }
};

///// CPP TYPE TRAITS

template <class T>
using Constructor = void(void*);
template <class T>
using Destructor = void(T*);

template <class T>
void default_construct (void* p) { new (p) T; }
template <class T>
void destroy (T* p) { p->~T(); }

inline void trivial_default_construct (void*) { }
inline void trivial_destroy (Mu*) { }

template <class T>
bool compare (const T& a, const T& b) { return a == b; };
template <class T>
void assign (T& a, const T& b) { a = b; }

///// DESCRIPTION HEADER

template <class T>
struct DescriptionFor : Description {
    Constructor<T>* default_construct;
    union {
        Destructor<T>* destroy;
        Destructor<Mu>* trivial_destroy;
    };
};

///// DESCRIPTORS

template <class T>
struct Descriptor : ComparableAddress { };
template <class T>
struct AttachedDescriptor : Descriptor<T> {
    constexpr u16 get_offset (DescriptionFor<T>& header) {
        return static_cast<ComparableAddress*>(this)
             - static_cast<ComparableAddress*>(&header);
    }
     // Emit this into the static data
    template <class Self>
    static constexpr Self make_static (const Self& self) { return self; }
};
template <class T>
struct DetachedDescriptor : Descriptor<T> {
     // Omit this from the static data
    template <class Self>
    static constexpr ComparableAddress make_static (const Self&) { return {}; }
};

template <class T>
struct NameDcr : DetachedDescriptor<T> {
    StaticString name;
};

 // We can't store the generated name in the description because it has to be
 // constexpr (we might be able to make it constinit, but it would be require a
 // lot of work and/or compromises).  So store it here.
template <class T> StaticString cached_name;
template <class T>
struct ComputedNameDcr : DetachedDescriptor<T> {
    StaticString* cache;
    NameFunc* f;
};

template <class T>
using ToTreeFunc = Tree(const T&);
template <class T>
struct ToTreeDcr : AttachedDescriptor<T> {
    ToTreeFunc<T>* f;
};

template <class T>
using FromTreeFunc = void(T&, const Tree&);
template <class T>
struct FromTreeDcr : AttachedDescriptor<T> {
    FromTreeFunc<T>* f;
};

template <class T>
struct BeforeFromTreeDcr : AttachedDescriptor<T> {
    FromTreeFunc<T>* f;
};

template <class T>
using SwizzleFunc = void(T&, const Tree&);
template <class T>
struct SwizzleDcr : AttachedDescriptor<T> {
    SwizzleFunc<T>* f;
};

template <class T>
using InitFunc = void(T&);
template <class T>
struct InitDcr : AttachedDescriptor<T> {
    InitFunc<T>* f;
    double priority;
};

template <class T>
struct FlagsDcr : DetachedDescriptor<T> {
    TypeFlags flags;
};

template <class T>
struct ValueDcr : ComparableAddress {
    Tree name;
};

 // Do some weirdness to ensure that the value is right after the name.
 // Using multiple alignas() specifiers picks the strictest (largest) one.
template <class T>
struct alignas(T) alignas(Tree) ValueDcrWithValue : ValueDcr<T> {
    static_assert(alignof(T) <= sizeof(Tree));
    alignas(T) alignas(Tree) T value;
};

template <class T>
struct ValueDcrWithPtr : ValueDcr<T> {
    const T* value;
};

template <class T>
using CompareFunc = bool(const T&, const T&);
template <class T>
using AssignFunc = void(T&, const T&);
template <class T>
struct ValuesDcr : AttachedDescriptor<T> {
    CompareFunc<T>* compare;
    AssignFunc<T>* assign;
    u16 n_values;
};
template <class T, class... Values>
struct ValuesDcrWith : ValuesDcr<T> {
    u16 offsets [sizeof...(Values)] {};
    Cat<Values...> values;
    constexpr ValuesDcrWith (const Values&... vs) :
        ValuesDcr<T>{{}, &compare<T>, &assign<T>, sizeof...(Values)},
        values(vs...)
    {
        for (u32 i = 0; i < sizeof...(Values); i++) {
            offsets[i] = static_cast<const ComparableAddress*>(
                values.template get<ValueDcr<T>>(i)
            ) - static_cast<const ComparableAddress*>(this);
        }
    }
    constexpr ValuesDcrWith (
        bool(* compare )(const T&, const T&),
        void(* assign )(T&, const T&),
        const Values&... vs
    ) :
        ValuesDcr<T>{{}, compare, assign, sizeof...(Values)},
        values(vs...)
    {
        for (u32 i = 0; i < sizeof...(Values); i++) {
            offsets[i] = static_cast<const ComparableAddress*>(
                values.template get<ValueDcr<T>>(i)
            ) - static_cast<const ComparableAddress*>(this);
        }
    }
    constexpr bool all_strings () {
        bool r = true;
        values.for_each([&](const auto& value){
            r &= value.name.form == Form::String;
        });
        return r;
    }
};

template <class T>
struct AttrDcr : ComparableAddress {
    StaticString key;
};
template <class T, class Acr>
struct AttrDcrWith : AttrDcr<T> {
    Acr acr;
    constexpr AttrDcrWith (StaticString k, const Acr& a) :
        AttrDcr<T>{{}, k},
        acr(constexpr_acr(a))
    {
         // Note that we can't validate flags here because they haven't been set
         // yet.  Do it in attr() in describe-base.inline.h instead.
    }
};
 // We can't put the default value after the acr because it has variable size,
 // and we can't put it between the key and the acr if it might not exist.  So
 // just put it before the beginning of the object instead!  It'll be fiiiiine
struct AttrDefault { Tree default_value; };
template <class T, class Acr>
struct AttrDefaultDcrWith : AttrDefault, AttrDcrWith<T, Acr> {
    constexpr AttrDefaultDcrWith (const Tree& t, const AttrDcrWith<T, Acr>& a) :
        AttrDefault(t), AttrDcrWith<T, Acr>(a)
    {
        AttrDcrWith<T, Acr>::acr.attr_flags |= AttrFlags::HasDefault;
    }
};

template <class T>
struct AttrsDcr : AttachedDescriptor<T> {
    u16 n_attrs;
};

template <class T, class... Attrs>
struct AttrsDcrWith : AttrsDcr<T> {
    u16 offsets [sizeof...(Attrs)] {};
    Cat<Attrs...> attrs;
    constexpr AttrsDcrWith (const Attrs&... as) :
        AttrsDcr<T>{{}, u16(sizeof...(Attrs))},
        attrs(as...)
    {
        for (u32 i = 0; i < sizeof...(Attrs); i++) {
            offsets[i] = static_cast<ComparableAddress*>(
                attrs.template get<AttrDcr<T>>(i)
            ) - static_cast<ComparableAddress*>(this);
        }
    }
    constexpr bool need_rebuild () {
        bool r = false;
        attrs.for_each([&](const auto& attr){
            r |= !!(attr.acr.attr_flags &
                (AttrFlags::Include|AttrFlags::HasDefault|
                 AttrFlags::CollapseOptional)
            );
        });
        return r;
    }
};

template <class T>
struct ElemDcr : ComparableAddress { };
template <class T, class Acr>
struct ElemDcrWith : ElemDcr<T> {
    Acr acr;
    constexpr ElemDcrWith (const Acr& a) :
        acr(constexpr_acr(a))
    {
         // Note that we can't validate flags here because they haven't been set
         // yet.  Instead do it in elem() in describe-base.inline.h
    }
};

template <class T>
struct ElemsDcr : AttachedDescriptor<T> {
    u16 n_elems;
};

template <class T, class... Elems>
struct ElemsDcrWith : ElemsDcr<T> {
    u16 offsets [sizeof...(Elems)] {};
    Cat<Elems...> elems;
    constexpr ElemsDcrWith (const Elems&... es) :
        ElemsDcr<T>{{}, u16(sizeof...(Elems))},
        elems(es...)
    {
        bool have_optional = false;
        bool have_invisible = false;
        u16 i = 0;
        elems.for_each([&]<class Elem>(const Elem& elem){
            if (!!(elem.acr.attr_flags & AttrFlags::Optional)) {
                have_optional = true;
            }
            else if (have_optional) {
                ERROR_cannot_have_non_optional_elem_after_optional_elem();
            }
            if (!!(elem.acr.attr_flags & AttrFlags::Invisible)) {
                have_invisible = true;
            }
            else if (have_invisible) {
                ERROR_cannot_have_non_invisible_elem_after_invisible_elem();
            }
            offsets[i++] = static_cast<const ComparableAddress*>(&elem)
                         - static_cast<const ComparableAddress*>(this);
        });
    }
};

template <class T>
struct KeysDcr : AttachedDescriptor<T> { };
template <class T, class Acr>
struct KeysDcrWith : KeysDcr<T> {
    static_assert(std::is_same_v<typename Acr::AcrFromType, T>);
    static_assert(std::is_same_v<typename Acr::AcrToType, AnyArray<AnyString>>);
    Acr acr;
    constexpr KeysDcrWith (const Acr& a) :
        acr(constexpr_acr(a))
    { }
};

template <class T>
using AttrFunc = AnyRef(T&, const AnyString&);
template <class T>
struct ComputedAttrsDcr : AttachedDescriptor<T> {
    AttrFunc<T>* f;
};

template <class T>
struct LengthDcr : AttachedDescriptor<T> { };
template <class T, class Acr>
struct LengthDcrWith : LengthDcr<T> {
    static_assert(std::is_same_v<typename Acr::AcrFromType, T>);
    static_assert(
        std::is_same_v<typename Acr::AcrToType, u32> ||
        std::is_same_v<typename Acr::AcrToType, u64>
    );
    Acr acr;
    constexpr LengthDcrWith (const Acr& a) :
        acr(constexpr_acr(a))
    { }
};

template <class T>
using ElemFunc = AnyRef(T&, u32);
template <class T>
struct ComputedElemsDcr : AttachedDescriptor<T> {
    ElemFunc<T>* f;
};

template <class T>
using DataFunc = AnyPtr(T&);
template <class T>
struct ContiguousElemsDcr : AttachedDescriptor<T> {
    DataFunc<T>* f;
};

template <class T>
struct DelegateDcr : AttachedDescriptor<T> { };
template <class T, class Acr>
struct DelegateDcrWith : DelegateDcr<T> {
    static_assert(std::is_same_v<typename Acr::AcrFromType, T>);
    Acr acr;
    constexpr DelegateDcrWith (const Acr& a) :
        acr(constexpr_acr(a))
    { }
};

template <class T>
struct DefaultConstructDcr : DetachedDescriptor<T> {
    void(* f )(void*);
};
template <class T>
struct DestroyDcr : DetachedDescriptor<T> {
    void(* f )(T*);
};

///// MAKING DESCRIPTIONS

template <class F>
constexpr void for_variadic (F) { }
template <class F, class Arg, class... Args>
constexpr void for_variadic (F f, Arg&& arg, Args&&... args) {
    f(std::forward<Arg>(arg));
    for_variadic(f, std::forward<Args>(args)...);
}

 // TODO: Prevent this from causing a template parameter deduction failure due
 // to missing make_static.  If make_description is given incorrect arguments,
 // we should try and make a custom compile-time error.
template <class T, class... Dcrs>
using FullDescription = Cat<
    DescriptionFor<T>,
    decltype(Dcrs::make_static(std::declval<Dcrs>()))...
>;

template <class T, class... Dcrs>
constexpr FullDescription<T, std::remove_cvref_t<Dcrs>...> make_description (
    const Dcrs&... dcrs
) {
    using Desc = FullDescription<T, std::remove_cvref_t<Dcrs>...>;

    static_assert(
        sizeof(T) <= u32(-1),
        "Cannot describe type larger the 4GB"
    );
    static_assert(
        sizeof(Desc) < 65536,
        "AYU_DESCRIBE description is too large (>64k)"
    );

    Desc desc (
        DescriptionFor<T>{},
        std::remove_cvref_t<Dcrs>::make_static(dcrs)...
    );
    auto& header = *desc.template get<DescriptionFor<T>>(0);
    header.cpp_size = sizeof(T);
    header.cpp_align = alignof(T);
    if constexpr (std::is_trivially_default_constructible_v<T>) {
        header.default_construct = &trivial_default_construct;
    }
    else if constexpr (requires { new (null) T; }) {
        header.default_construct = &default_construct<T>;
    }
    else {
        header.default_construct = null;
    }
    if constexpr (std::is_trivially_destructible_v<T>) {
        header.trivial_destroy = &trivial_destroy;
    }
     // Make sure to use T& here so that arrays don't decay
    else if constexpr (requires (T& v) { v.~T(); }) {
        header.destroy = &destroy<T>;
    }
    else {
        header.destroy = null;
    }

    bool have_default_construct = false;
    bool have_destroy = false;
    bool have_name = false;
    bool have_computed_name = false;
    bool have_to_tree = false;
    bool have_from_tree = false;
    bool have_before_from_tree = false;
    bool have_swizzle = false;
    bool have_init = false;
    bool have_flags = false;
    bool have_values = false;
    bool have_keys = false;
    bool have_attrs = false;
    bool have_computed_attrs = false;
    bool have_length = false;
    bool have_elems = false;
    bool have_computed_elems = false;
    bool have_contiguous_elems = false;
    bool have_delegate = false;

    for_variadic([&]<class Dcr>(const Dcr& dcr){
         // Warning: We're operating on objects that may have been moved from.
         // To access an AttachedDescriptor, use desc.template get
         // To access a DetachedDescriptor, use dcr
        if constexpr (std::is_base_of_v<DefaultConstructDcr<T>, Dcr>) {
            if (have_default_construct) {
                ERROR_duplicate_descriptors<Dcr>();
            }
            have_default_construct = true;
            header.default_construct = dcr.f;
        }
        else if constexpr (std::is_base_of_v<DestroyDcr<T>, Dcr>) {
            if (have_destroy) {
                ERROR_duplicate_descriptors<Dcr>();
            }
            have_destroy = true;
            header.destroy = dcr.f;
        }
        else if constexpr (std::is_base_of_v<NameDcr<T>, Dcr>) {
            if (have_computed_name) {
                ERROR_cannot_have_non_computed_name_after_computed_name();
            }
             // Allow duplicate name descriptors.  A later one overrides an
             // earlier one.
            have_name = true;
            header.name = dcr.name;
        }
        else if constexpr (std::is_base_of_v<ComputedNameDcr<T>, Dcr>) {
            if (have_computed_name) {
                ERROR_duplicate_descriptors<ComputedNameDcr<T>>();
            }
             // Allow computed_name to override non-computed name.
            have_computed_name = true;
            header.flags |= DescFlags::NameComputed;
            header.computed_name.cache = dcr.cache;
            header.computed_name.f = dcr.f;
        }
        else if constexpr (std::is_base_of_v<ToTreeDcr<T>, Dcr>) {
#define AYU_APPLY_OFFSET(dcr_type, dcr_name) \
            if (have_##dcr_name) { \
                ERROR_duplicate_descriptors<dcr_type<T>>(); \
            } \
            have_##dcr_name = true; \
            header.dcr_name##_offset = \
                desc.template get<dcr_type<T>>(0)->get_offset(header);
            AYU_APPLY_OFFSET(ToTreeDcr, to_tree)
        }
        else if constexpr (std::is_base_of_v<FromTreeDcr<T>, Dcr>) {
            AYU_APPLY_OFFSET(FromTreeDcr, from_tree)
        }
        else if constexpr (std::is_base_of_v<BeforeFromTreeDcr<T>, Dcr>) {
            AYU_APPLY_OFFSET(BeforeFromTreeDcr, before_from_tree)
        }
        else if constexpr (std::is_base_of_v<SwizzleDcr<T>, Dcr>) {
            AYU_APPLY_OFFSET(SwizzleDcr, swizzle)
        }
        else if constexpr (std::is_base_of_v<InitDcr<T>, Dcr>) {
            AYU_APPLY_OFFSET(InitDcr, init)
        }
        else if constexpr (std::is_base_of_v<FlagsDcr<T>, Dcr>) {
            if (have_flags) {
                ERROR_duplicate_descriptors<FlagsDcr<T>>();
            }
            have_flags = true;
            header.type_flags = dcr.flags;
        }
        else if constexpr (std::is_base_of_v<ValuesDcr<T>, Dcr>) {
            AYU_APPLY_OFFSET(ValuesDcr, values)
            Dcr* values = desc.template get<Dcr>(0);
            if (values->all_strings()) {
                header.flags |= DescFlags::ValuesAllStrings;
            }
        }
        else if constexpr (std::is_base_of_v<AttrsDcr<T>, Dcr>) {
            if (have_keys || have_computed_attrs) {
                ERROR_attrs_cannot_be_combined_with_keys_and_computed_attrs();
            }
            AYU_APPLY_OFFSET(AttrsDcr, attrs)
            if (!(header.flags & DescFlags::Preference)) {
                header.flags |= DescFlags::PreferObject;
            }
            Dcr* attrs = desc.template get<Dcr>(0);
            if (attrs->need_rebuild()) {
                header.flags |= DescFlags::AttrsNeedRebuild;
            }
        }
        else if constexpr (std::is_base_of_v<KeysDcr<T>, Dcr>) {
            if (have_attrs) {
                ERROR_attrs_cannot_be_combined_with_keys_and_computed_attrs();
            }
            AYU_APPLY_OFFSET(KeysDcr, keys)
            if (!(header.flags & DescFlags::Preference)) {
                header.flags |= DescFlags::PreferObject;
            }
        }
        else if constexpr (std::is_base_of_v<ComputedAttrsDcr<T>, Dcr>) {
            if (have_attrs) {
                ERROR_attrs_cannot_be_combined_with_keys_and_computed_attrs();
            }
            AYU_APPLY_OFFSET(ComputedAttrsDcr, computed_attrs)
            if (!(header.flags & DescFlags::Preference)) {
                header.flags |= DescFlags::PreferObject;
            }
        }
        else if constexpr (std::is_base_of_v<ElemsDcr<T>, Dcr>) {
            if (have_length || have_computed_elems || have_contiguous_elems) {
                ERROR_elems_cannot_be_combined_with_length_and_computed_elems();
            }
            AYU_APPLY_OFFSET(ElemsDcr, elems)
            if (!(header.flags & DescFlags::Preference)) {
                header.flags |= DescFlags::PreferArray;
            }
        }
        else if constexpr (std::is_base_of_v<LengthDcr<T>, Dcr>) {
            if (have_elems) {
                ERROR_elems_cannot_be_combined_with_length_and_computed_elems();
            }
            AYU_APPLY_OFFSET(LengthDcr, length)
            if (!(header.flags & DescFlags::Preference)) {
                header.flags |= DescFlags::PreferArray;
            }
        }
        else if constexpr (std::is_base_of_v<ComputedElemsDcr<T>, Dcr>) {
            if (have_elems) {
                ERROR_elems_cannot_be_combined_with_length_and_computed_elems();
            }
            if (have_contiguous_elems) {
                ERROR_cannot_have_both_computed_and_contiguous_elems();
            }
            AYU_APPLY_OFFSET(ComputedElemsDcr, computed_elems)
            if (!(header.flags & DescFlags::Preference)) {
                header.flags |= DescFlags::PreferArray;
            }
        }
        else if constexpr (std::is_base_of_v<ContiguousElemsDcr<T>, Dcr>) {
            if (have_elems) {
                ERROR_elems_cannot_be_combined_with_length_and_computed_elems();
            }
            if (have_computed_elems) {
                ERROR_cannot_have_both_computed_and_contiguous_elems();
            }
            AYU_APPLY_OFFSET(ContiguousElemsDcr, contiguous_elems)
            header.flags |= DescFlags::ElemsContiguous;
            if (!(header.flags & DescFlags::Preference)) {
                header.flags |= DescFlags::PreferArray;
            }
        }
        else if constexpr (std::is_base_of_v<DelegateDcr<T>, Dcr>) {
            AYU_APPLY_OFFSET(DelegateDcr, delegate)
        }
#undef AYU_APPLY_OFFSET
        else {
            ERROR_element_is_not_a_descriptor_for_this_type();
        }
    }, dcrs...);

    if ((have_keys && !have_computed_attrs) ||
        (have_computed_attrs && !have_keys)
    ) {
        ERROR_keys_and_computed_attrs_must_be_together();
    }
    if (have_length) {
        if (!have_computed_elems && !have_contiguous_elems) {
            ERROR_cannot_have_length_without_computed_or_contiguous_elems();
        }
    }
    else if (have_computed_elems || have_contiguous_elems) {
        ERROR_cannot_have_computed_or_contiguous_elems_without_length();
    }
    if (!have_name && !have_computed_name) {
         // If this error happens, you probably used AYU_DESCRIBE_TEMPLATE but
         // forgot to specify a computed_name().
        ERROR_description_doesnt_have_name_or_computed_name();
    }

    return desc;
}

} // namespace ayu::in
