
namespace ayu {

template <class T>
constexpr auto _AYU_DescribeBase<T>::name (StaticString(* f )()) {
    return in::NameDcr<T>{{}, f};
}

template <class T>
constexpr auto _AYU_DescribeBase<T>::to_tree (in::ToTreeFunc<T>* f) {
    return in::ToTreeDcr<T>{{}, f};
}
template <class T>
constexpr auto _AYU_DescribeBase<T>::from_tree (in::FromTreeFunc<T>* f) {
    return in::FromTreeDcr<T>{{}, f};
}
template <class T>
constexpr auto _AYU_DescribeBase<T>::swizzle (in::SwizzleFunc<T>* f) {
    return in::SwizzleDcr<T>{{}, f};
}
template <class T>
constexpr auto _AYU_DescribeBase<T>::init (in::InitFunc<T>* f) {
    return in::InitDcr<T>{{}, f};
}

template <class T>
constexpr auto _AYU_DescribeBase<T>::default_construct (void(* f )(void*)) {
    return in::DefaultConstructDcr<T>{{}, f};
}
template <class T>
constexpr auto _AYU_DescribeBase<T>::destroy (void(* f )(T*)) {
    return in::DestroyDcr<T>{{}, f};
}

template <class T>
template <class... Values>
    requires (requires (T v) { v == v; v = v; })
constexpr auto _AYU_DescribeBase<T>::values (const Values&... vs) {
    return in::ValuesDcrWith<T, Values...>(vs...);
}
template <class T>
template <class... Values>
constexpr auto _AYU_DescribeBase<T>::values_custom (
    in::CompareFunc<T>* compare,
    in::AssignFunc<T>* assign,
    const Values&... vs
) {
    return in::ValuesDcrWith<T, Values...>(compare, assign, vs...);
}
template <class T>
template <class N>
    requires (requires (T v) { T(move(v)); })
constexpr auto _AYU_DescribeBase<T>::value (const N& n, T&& v) {
     // Be aggressive about converting to StaticString to make sure nothing goes
     // through the non-constexpr path.
    if constexpr (
        !requires { Null(n); } &&
        requires { StaticString(n); }
    ) {
        return in::ValueDcrWithValue<T>{
            {{}, Tree(StaticString(n)), null}, move(v)
        };
    }
    else return in::ValueDcrWithValue<T>{{{}, Tree(n), null}, move(v)};
}
 // Forwarding references don't seem to work in the above T&& overload.
template <class T>
template <class N>
    requires (requires (const T& v) { T(v); })
constexpr auto _AYU_DescribeBase<T>::value (const N& n, const T& v) {
    if constexpr (
        !requires { Null(n); } &&
        requires { StaticString(n); }
    ) {
        return in::ValueDcrWithValue<T>{
            {{}, Tree(StaticString(n)), null}, move(v)
        };
    }
    else return {in::ValueDcrWithValue<T>{{}, Tree(n), null}, move(v)};
}
template <class T>
template <class N>
constexpr auto _AYU_DescribeBase<T>::value_pointer (const N& n, const T* p) {
    if constexpr (
        !requires { Null(n); } &&
        requires { StaticString(n); }
    ) {
        return in::ValueDcr<T>{{}, Tree(StaticString(n)), p};
    }
    else return in::ValueDcr<T>{{}, Tree(n), p};
}

template <class T>
template <class... Attrs>
constexpr auto _AYU_DescribeBase<T>::attrs (const Attrs&... as) {
    return in::AttrsDcrWith<T, Attrs...>(as...);
}
template <class T>
template <class Acr>
constexpr auto _AYU_DescribeBase<T>::attr (
    StaticString key, const Acr& acr, in::AttrFlags flags
) {
     // Implicit member().
    if constexpr (std::is_member_object_pointer_v<Acr>) {
        return attr(key, _AYU_DescribeBase<T>::member(acr), flags);
    }
    else {
        static_assert(
            std::is_same_v<typename Acr::AcrFromType, T>,
            "Second argument to attr() is not an accessor of this type"
        );
        auto r = in::AttrDcrWith<T, Acr>(key, acr);
        r.acr.attr_flags = flags;
        return r;
    }
}
template <class T>
template <class... Elems>
constexpr auto _AYU_DescribeBase<T>::elems (const Elems&... es) {
    return in::ElemsDcrWith<T, Elems...>(es...);
}
template <class T>
template <class Acr>
constexpr auto _AYU_DescribeBase<T>::elem (
    const Acr& acr, in::AttrFlags flags
) {
    if constexpr (std::is_member_object_pointer_v<Acr>) {
        return elem(_AYU_DescribeBase<T>::member(acr), flags);
    }
    else {
        static_assert(
            std::is_same_v<typename Acr::AcrFromType, T>,
            "First argument to elem() is not an accessor of this type"
        );
        auto r = in::ElemDcrWith<T, Acr>(acr);
        r.acr.attr_flags = flags;
        return r;
    }
}
template <class T>
template <class Acr>
constexpr auto _AYU_DescribeBase<T>::keys (const Acr& acr) {
    return in::KeysDcrWith<T, Acr>(acr);
}
template <class T>
constexpr auto _AYU_DescribeBase<T>::computed_attrs (in::AttrFunc<T>* f) {
    return in::ComputedAttrsDcr<T>{{}, f};
}
template <class T>
template <class Acr>
constexpr auto _AYU_DescribeBase<T>::length (const Acr& acr) {
    return in::LengthDcrWith<T, Acr>(acr);
}
template <class T>
constexpr auto _AYU_DescribeBase<T>::computed_elems (in::ElemFunc<T>* f) {
    return in::ComputedElemsDcr<T>{{}, f};
}
template <class T>
template <class Acr>
constexpr auto _AYU_DescribeBase<T>::delegate (const Acr& acr) {
    return in::DelegateDcrWith<T, Acr>(acr);
}

template <class T>
template <class T2, class M>
constexpr auto _AYU_DescribeBase<T>::member (
    M T2::* mp, in::AcrFlags flags
) {
    return in::MemberAcr2<T, M>(mp, flags);
}
template <class T>
template <class T2, class M>
constexpr auto _AYU_DescribeBase<T>::const_member (
    const M T2::* mp, in::AcrFlags flags
) {
    return in::MemberAcr2<T, M>(
        const_cast<M T::*>(mp), flags | in::AcrFlags::Readonly
    );
}
template <class T>
template <class B>
    requires (requires (T* t, B* b) { b = t; t = static_cast<T*>(b); })
constexpr auto _AYU_DescribeBase<T>::base (
    in::AcrFlags flags
) {
    return in::BaseAcr2<T, B>(flags);
}
template <class T>
template <class M>
constexpr auto _AYU_DescribeBase<T>::ref_func (
    M&(* f )(T&),
    in::AcrFlags flags
) {
    return in::RefFuncAcr2<T, M>(f, flags);
}
template <class T>
template <class M>
constexpr auto _AYU_DescribeBase<T>::const_ref_func (
    const M&(* f )(const T&),
    in::AcrFlags flags
) {
    return in::ConstRefFuncAcr2<T, M>(f, flags);
}
template <class T>
template <class M>
constexpr auto _AYU_DescribeBase<T>::const_ref_funcs (
    const M&(* g )(const T&),
    void(* s )(T&, const M&),
    in::AcrFlags flags
) {
    return in::RefFuncsAcr2<T, M>(g, s, flags);
}
template <class T>
template <class M>
    requires (requires (M m) { M(move(m)); })
constexpr auto _AYU_DescribeBase<T>::value_func (
    M(* f )(const T&),
    in::AcrFlags flags
) {
    return in::ValueFuncAcr2<T, M>(f, flags);
}
template <class T>
template <class M>
    requires (requires (M m) { M(move(m)); })
constexpr auto _AYU_DescribeBase<T>::value_funcs (
    M(* g )(const T&),
    void(* s )(T&, M),
    in::AcrFlags flags
) {
    return in::ValueFuncsAcr2<T, M>(g, s, flags);
}
template <class T>
template <class M>
    requires (requires (M m) { M(move(m)); })
constexpr auto _AYU_DescribeBase<T>::mixed_funcs (
    M(* g )(const T&),
    void(* s )(T&, const M&),
    in::AcrFlags flags
) {
    return in::MixedFuncsAcr2<T, M>(g, s, flags);
}

template <class T>
template <class M>
    requires (requires (T t, M m) { t = m; m = t; })
constexpr auto _AYU_DescribeBase<T>::assignable (
    in::AcrFlags flags
) {
    return in::AssignableAcr2<T, M>(flags);
}

template <class T>
template <class M>
    requires (requires (M m) { M(move(m)); })
constexpr auto _AYU_DescribeBase<T>::constant (
    M&& v, in::AcrFlags flags
) {
    return in::ConstantAcr2<T, M>(move(v), flags);
}
template <class T>
template <class M>
constexpr auto _AYU_DescribeBase<T>::constant_pointer (
    const M* p, in::AcrFlags flags
) {
    return in::ConstantPointerAcr2<T, M>(p, flags);
}

 // This one is not constexpr, so it is only valid in computed_attrs,
 // computed_elems, or reference_func.
template <class T>
template <class M>
    requires (requires (M m) { M(move(m)); m.~M(); })
auto _AYU_DescribeBase<T>::variable (
    M&& v, in::AcrFlags flags
) {
    return in::VariableAcr2<T, M>(move(v), flags);
}

template <class T>
constexpr auto _AYU_DescribeBase<T>::reference_func (
    Reference(* f )(T&), in::AcrFlags flags
) {
    return in::ReferenceFuncAcr2<T>(f, flags);
}

template <class T>
template <class... Dcrs>
constexpr auto _AYU_DescribeBase<T>::_ayu_describe (
    StaticString name, const Dcrs&... dcrs
) {
    return in::make_description<T, Dcrs...>(name, dcrs...);
}

} // namespace ayu

#ifdef AYU_DISCARD_ALL_DESCRIPTIONS
#define AYU_DESCRIBE(...)
#define AYU_DESCRIBE_TEMPLATE(...)
#define AYU_DESCRIBE_INSTANTIATE(...)
#else

 // Stringify name as early as possible to avoid macro expansion
 // TODO make description constinit so names can be generated at runtime
#define AYU_DESCRIBE_BEGIN(T) AYU_DESCRIBE_BEGIN_NAME(T, #T)
#define AYU_DESCRIBE_BEGIN_NAME(T, name) \
template <> \
struct ayu_desc::_AYU_Describe<T> : ayu::_AYU_DescribeBase<T> { \
    using desc = ayu::_AYU_DescribeBase<T>; \
    static constexpr auto _ayu_full_description = ayu::_AYU_DescribeBase<T>::_ayu_describe(name

#define AYU_DESCRIBE_END(T) \
    ); \
    static const ayu::in::Description* const _ayu_description; \
}; \
const ayu::in::Description* const ayu_desc::_AYU_Describe<T>::_ayu_description = \
    ayu::in::register_description( \
        _ayu_full_description.template get<ayu::in::Description>(0) \
    );

#define AYU_DESCRIBE(T, ...) AYU_DESCRIBE_NAME(T, #T, __VA_ARGS__)
#define AYU_DESCRIBE_NAME(T, name, ...) \
AYU_DESCRIBE_BEGIN_NAME(T, name) \
    __VA_OPT__(,) __VA_ARGS__ \
AYU_DESCRIBE_END(T)

#define AYU_DESCRIBE_TEMPLATE_PARAMS(...) <__VA_ARGS__>
#define AYU_DESCRIBE_TEMPLATE_TYPE(...) __VA_ARGS__

#define AYU_DESCRIBE_TEMPLATE_BEGIN(params, T) \
template params \
struct ayu_desc::_AYU_Describe<T> : ayu::_AYU_DescribeBase<T> { \
    using desc = ayu::_AYU_DescribeBase<T>; \
    static constexpr auto _ayu_full_description = desc::_ayu_describe("",

#define AYU_DESCRIBE_TEMPLATE_END(params, T) \
    ); \
    static const ayu::in::Description* const _ayu_description; \
}; \
template params \
const ayu::in::Description* const ayu_desc::_AYU_Describe<T>::_ayu_description = \
    ayu::in::register_description( \
        _ayu_full_description.template get<ayu::in::Description>(0) \
    );

#define AYU_DESCRIBE_ESCAPE(...) __VA_ARGS__

#define AYU_DESCRIBE_TEMPLATE(params, T, ...) \
AYU_DESCRIBE_TEMPLATE_BEGIN(AYU_DESCRIBE_ESCAPE(params), AYU_DESCRIBE_ESCAPE(T)) \
    __VA_ARGS__ \
AYU_DESCRIBE_TEMPLATE_END(AYU_DESCRIBE_ESCAPE(params), AYU_DESCRIBE_ESCAPE(T))

 // Force instantiation without screwing with the address sanitizer
 // Use -Wno-unused-value
#define AYU_DESCRIBE_INSTANTIATE(T) \
static_assert((&ayu_desc::_AYU_Describe<T>::_ayu_description, 1));

#define AYU_FRIEND_DESCRIBE(T) \
    friend struct ::ayu_desc::_AYU_Describe<T>;

#endif
