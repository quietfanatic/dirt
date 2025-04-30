
namespace ayu {

static void ERROR_conflicting_flags_on_attr () { }
static void ERROR_elem_cannot_have_collapse_optional_flag () { }

template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::name (StaticString n) {
    return in::NameDcr<T>{{}, n};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::computed_name (in::NameFunc* f) {
    return in::ComputedNameDcr<T>{{}, &in::cached_name<T>, f};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::to_tree (in::ToTreeFunc<T>* f) {
    return in::ToTreeDcr<T>{{}, f};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::from_tree (in::FromTreeFunc<T>* f) {
    return in::FromTreeDcr<T>{{}, f};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::before_from_tree (in::FromTreeFunc<T>* f) {
    return in::BeforeFromTreeDcr<T>{{}, f};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::swizzle (in::SwizzleFunc<T>* f) {
    return in::SwizzleDcr<T>{{}, f};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::init (in::InitFunc<T>* f, double pri) {
    return in::InitDcr<T>{{}, f, pri};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::default_construct (void(* f )(void*)) {
    return in::DefaultConstructDcr<T>{{}, f};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::destroy (void(* f )(T*)) {
    return in::DestroyDcr<T>{{}, f};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::flags (in::TypeFlags f) {
    return in::FlagsDcr<T>{{}, f};
}
template <Describable T>
template <ValueDcrFor<T>... Values> requires (
    requires (T v) { v == v; v = v; }
) constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::values (const Values&... vs) {
    return in::ValuesDcrWith<T, Values...>(vs...);
}
template <Describable T>
template <ValueDcrFor<T>... Values> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::values_custom (
    in::CompareFunc<T>* compare,
    in::AssignFunc<T>* assign,
    const Values&... vs
) {
    return in::ValuesDcrWith<T, Values...>(
        compare, assign, vs...
    );
}
template <Describable T>
template <ConstructsTree N> requires (Copyable<T>) constexpr
ValueDcrFor<T> auto AYU_DescribeBase<T>::value (const N& n, const T& v) {
    Tree name;
    if constexpr (
        !requires { Null(n); } &&
        requires { StaticString(n); }
    ) {
        name = Tree(StaticString(n));
    }
    else name = Tree(n);
    name.flags &= ~TreeFlags::ValueIsPtr;
    return in::ValueDcrWithValue<T>{{{}, name}, v};
}
template <Describable T>
template <ConstructsTree N> constexpr
ValueDcrFor<T> auto AYU_DescribeBase<T>::value_ptr (const N& n, const T* p) {
    Tree name;
    if constexpr (
        !requires { Null(n); } &&
        requires { StaticString(n); }
    ) {
        name = Tree(StaticString(n));
    }
    else name = Tree(n);
    name.flags |= TreeFlags::ValueIsPtr;
    return in::ValueDcrWithPtr<T>{{{}, name}, p};
}
template <Describable T>
template <AttrDcrFor<T>... Attrs> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::attrs (const Attrs&... as) {
    return in::AttrsDcrWith<T, Attrs...>(as...);
}
template <Describable T>
template <AccessorFrom<T> Acr> constexpr
AttrDcrFor<T> auto AYU_DescribeBase<T>::attr (
    StaticString key, const Acr& acr, AttrFlags flags
) {
    u32 count = flags % in::AttrFlags::Optional
              + flags % in::AttrFlags::Collapse
              + flags % in::AttrFlags::CollapseOptional;
    if (count > 1) {
        ERROR_conflicting_flags_on_attr();
    }
    auto r = in::AttrDcrWith<T, Acr>(key, acr);
    r.acr.attr_flags = flags;
    return r;
}
template <Describable T>
template <AccessorFrom<T> Acr, ConstructsTree Default> constexpr
AttrDcrFor<T> auto AYU_DescribeBase<T>::attr_default (
    StaticString key, const Acr& acr, const Default& def, in::AttrFlags flags
) {
    return in::AttrDefaultDcrWith<T, Acr>(
        Tree(def),
        AYU_DescribeBase<T>::attr(key, acr, flags)
    );
}
template <Describable T>
template <ElemDcrFor<T>... Elems> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::elems (const Elems&... es) {
    return in::ElemsDcrWith<T, Elems...>(es...);
}
template <Describable T>
template <AccessorFrom<T> Acr> constexpr
ElemDcrFor<T> auto AYU_DescribeBase<T>::elem (
    const Acr& acr, in::AttrFlags flags
) {
    if (flags % in::AttrFlags::CollapseOptional) {
        ERROR_elem_cannot_have_collapse_optional_flag();
    }
    auto r = in::ElemDcrWith<T, Acr>(acr);
    r.acr.attr_flags = flags;
    return r;
}
template <Describable T>
template <AccessorFrom<T> Acr> requires (
    AccessorTo<Acr, AnyArray<AnyString>>
) constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::keys (const Acr& acr) {
    return in::KeysDcrWith<T, Acr>(acr);
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::computed_attrs (in::AttrFunc<T>* f) {
    return in::ComputedAttrsDcr<T>{{}, f};
}
template <Describable T>
template <AccessorFrom<T> Acr> requires (
    AccessorTo<Acr, u32> || AccessorTo<Acr, u64>
) constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::length (const Acr& acr) {
    return in::LengthDcrWith<T, Acr>(acr);
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::computed_elems (in::ElemFunc<T>* f) {
    return in::ComputedElemsDcr<T>{{}, f};
}
template <Describable T> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::contiguous_elems (in::DataFunc<T>* f) {
    return in::ContiguousElemsDcr<T>{{}, f};
}
template <Describable T>
template <AccessorFrom<T> Acr> constexpr
DescriptorFor<T> auto AYU_DescribeBase<T>::delegate (const Acr& acr) {
    return in::DelegateDcrWith<T, Acr>(acr);
}

template <Describable T>
template <SameOrBase<T> T2, Describable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::member (
    M T2::* mp, AcrFlags flags
) {
     // Sadly we can't use NoopAcr to save a word if mp is 0, because we can't
     // check mp in an if constexpr because it's a parameter, so we can't change
     // the return type of this function based on it.
    return in::MemberAcr<T, M>(mp, flags);
}
template <Describable T>
template <SameOrBase<T> T2, Describable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::const_member (
    const M T2::* mp, AcrFlags flags
) {
    return in::MemberAcr<T, M>(
        const_cast<M T::*>(mp), flags | in::AcrFlags::Readonly
    );
}
template <Describable T>
template <Describable M>
    requires (requires (T* t, M* m) { m = t; t = static_cast<T*>(m); }) constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::base (AcrFlags flags) {
    if constexpr (
        static_cast<M*>((T*)null) == null
    ) {
         // BaseAcr is kinda heavy because it may have to deal with virtual
         // bases, so if the base is the first base (offset 0), use a special
         // Acr that doesn't need to store any data.
        return in::NoopAcr<T, M>(flags);
    }
    else return in::BaseAcr<T, M>(flags);
}
template <Describable T>
template <Describable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::ref_func (
    M&(* f )(T&),
    AcrFlags flags
) {
    return in::RefFuncAcr<T, M>(f, flags);
}
template <Describable T>
template <Describable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::const_ref_func (
    const M&(* f )(const T&),
    AcrFlags flags
) {
    return in::ConstRefFuncAcr<T, M>(f, flags);
}
template <Describable T>
template <Describable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::const_ref_funcs (
    const M&(* g )(const T&),
    void(* s )(T&, const M&),
    AcrFlags flags
) {
    return in::RefFuncsAcr<T, M>(g, s, flags);
}
template <Describable T>
template <Movable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::value_func (
    M(* f )(const T&),
    AcrFlags flags
) {
    return in::ValueFuncAcr<T, M>(f, flags);
}
template <Describable T>
template <Movable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::value_funcs (
    M(* g )(const T&),
    void(* s )(T&, M),
    AcrFlags flags
) {
    return in::ValueFuncsAcr<T, M>(g, s, flags);
}
template <Describable T>
template <Movable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::mixed_funcs (
    M(* g )(const T&),
    void(* s )(T&, const M&),
    AcrFlags flags
) {
    return in::MixedFuncsAcr<T, M>(g, s, flags);
}

 // TODO: optimize for pointers
template <Describable T>
template <Describable M>
    requires (requires (T t, M m) { t = m; m = t; }) constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::assignable (
    AcrFlags flags
) {
    return in::AssignableAcr<T, M>(flags);
}

template <Describable T>
template <Copyable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::constant (
    const M& v, AcrFlags flags
) {
    return in::ConstantAcr<T, M>(move(v), flags);
}
template <Describable T>
template <Describable M> constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::constant_ptr (
    const M* p, AcrFlags flags
) {
    return in::ConstantPtrAcr<T, M>(p, flags);
}

template <Describable T>
template <Movable M> // not constexpr
AccessorFromTo<T, M> auto AYU_DescribeBase<T>::variable (
    M&& v, AcrFlags flags
) {
    return in::VariableAcr<T, M>(move(v), flags);
}

template <Describable T> constexpr
AccessorFrom<T> auto AYU_DescribeBase<T>::anyref_func (
    Function<AnyRef(T&)>* f, AcrFlags flags
) {
    return in::AnyRefFuncAcr<T>(f, flags);
}
template <Describable T> constexpr
AccessorFrom<T> auto AYU_DescribeBase<T>::anyptr_func (
    Function<AnyPtr(T&)>* f, AcrFlags flags
) {
    return in::AnyPtrFuncAcr<T>(f, flags);
}

template <Describable T>
template <DescriptorFor<T>... Dcrs>
constexpr auto AYU_DescribeBase<T>::AYU_describe (
    const Dcrs&... dcrs
) {
    return in::make_description<T, Dcrs...>(dcrs...);
}

} // namespace ayu

#ifdef AYU_DISCARD_ALL_DESCRIPTIONS
#define AYU_DESCRIBE(...)
#define AYU_DESCRIBE_TEMPLATE(...)
#define AYU_DESCRIBE_INSTANTIATE(...)
#else

#ifdef __GNUC__
#define AYU_DESCRIPTION_CONST constexpr
#define AYU_DO_INIT(init) &init
#else
#define AYU_DESCRIPTION_CONST const
#define AYU_DO_INIT(init) init()
#endif

 // Stringify name as early as possible to avoid macro expansion
#define AYU_DESCRIBE_BEGIN(T) AYU_DESCRIBE_BEGIN_NAME(T, #T)
#define AYU_DESCRIBE_BEGIN_NAME(T, name_) \
template <> \
struct AYU_Describe<T> : ayu::AYU_DescribeBase<T> { \
    using desc = ayu::AYU_DescribeBase<T>; \
    static constexpr auto AYU_specification = ayu::AYU_DescribeBase<T>::AYU_describe( \
        name(name_)

#define AYU_DESCRIBE_END(T) \
    ); \
    static const AYU_Description<T> AYU_description; \
    [[gnu::constructor]] static void init () { \
        ayu::in::register_description(&AYU_description); \
    } \
}; \
template <> \
struct AYU_Description<T> : decltype(AYU_Describe<T>::AYU_specification) { }; \
AYU_DESCRIPTION_CONST AYU_Description<T> AYU_Describe<T>::AYU_description { \
    (AYU_DO_INIT(init), AYU_specification) \
};

#define AYU_DESCRIBE(T, ...) AYU_DESCRIBE_NAME(T, #T, __VA_ARGS__)
#define AYU_DESCRIBE_NAME(T, name, ...) \
AYU_DESCRIBE_BEGIN_NAME(T, name) \
    __VA_OPT__(,) __VA_ARGS__ \
AYU_DESCRIBE_END(T)

#define AYU_DESCRIBE_TEMPLATE_PARAMS(...) <__VA_ARGS__>
#define AYU_DESCRIBE_TEMPLATE_TYPE(...) __VA_ARGS__

#define AYU_DESCRIBE_TEMPLATE_BEGIN(params, T) \
template params \
struct AYU_Describe<T> : ayu::AYU_DescribeBase<T> { \
    using desc = ayu::AYU_DescribeBase<T>; \
    static constexpr auto AYU_specification = desc::AYU_describe(

#define AYU_DESCRIBE_TEMPLATE_END(params, T) \
    ); \
    static const AYU_Description<T> AYU_description; \
    [[gnu::constructor]] static void init () { \
        ayu::in::register_description(&AYU_description); \
    } \
}; \
template params \
struct AYU_Description<T> : decltype(AYU_Describe<T>::AYU_specification) { }; \
template params \
AYU_DESCRIPTION_CONST AYU_Description<T> AYU_Describe<T>::AYU_description { \
    (AYU_DO_INIT(init), AYU_specification) \
};

#define AYU_DESCRIBE_ESCAPE(...) __VA_ARGS__

#define AYU_DESCRIBE_TEMPLATE(params, T, ...) \
AYU_DESCRIBE_TEMPLATE_BEGIN(AYU_DESCRIBE_ESCAPE(params), AYU_DESCRIBE_ESCAPE(T)) \
    __VA_ARGS__ \
AYU_DESCRIBE_TEMPLATE_END(AYU_DESCRIBE_ESCAPE(params), AYU_DESCRIBE_ESCAPE(T))

 // Force instantiation.  I can't believe it took me this long to learn that
 // there is an official way to do this. (template keyword without <>)
#define AYU_DESCRIBE_INSTANTIATE(T) \
template const AYU_Description<T> AYU_Describe<T>::AYU_description;

#define AYU_FRIEND_DESCRIBE(T) \
    friend struct AYU_Describe<T>;

#endif
