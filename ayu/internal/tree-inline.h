#pragma once
#include <cstring>

namespace ayu {
namespace in {

using TreeRep = int8;
enum : TreeRep {
    REP_UNDEFINED = 0,
    REP_NULL = 1,
    REP_BOOL = 2,
    REP_INT64 = 3,
    REP_DOUBLE = 4,
    REP_STATICSTRING = 5,
     // Types requiring reference counting
    REP_SHAREDSTRING = -1,
    REP_ARRAY = -2,
    REP_OBJECT = -3,
    REP_ERROR = -4,
};

NOINLINE
void delete_Tree_data (TreeRef) noexcept;

[[noreturn]]
void raise_TreeWrongForm (TreeRef, Form);
[[noreturn]]
void raise_TreeCantRepresent (StaticString, TreeRef);

} // in

constexpr Tree::Tree () :
    form(Form::Undefined), rep(0), flags(0), length(0), data{.as_int64 = 0}
{ }
constexpr Tree::Tree (Null, TreeFlags f) :
    form(Form::Null), rep(in::REP_NULL), flags(f), length(0), data{.as_int64 = 0}
{ }
 // Use .as_int64 to write all of data
template <class T> requires (std::is_same_v<T, bool>)
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Bool), rep(in::REP_BOOL), flags(f), length(0), data{.as_int64 = v}
{ }
template <class T> requires (
    std::is_integral_v<T> &&
    !std::is_same_v<T, bool> && !std::is_same_v<T, char>
)
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Number), rep(in::REP_INT64), flags(f), length(0), data{.as_int64 = int64(v)}
{ }
template <class T> requires (std::is_floating_point_v<T>)
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Number), rep(in::REP_DOUBLE), flags(f), length(0), data{.as_double = v}
{ }
constexpr Tree::Tree (AnyString v, TreeFlags f) :
    form(Form::String), rep(v.owned() ? in::REP_SHAREDSTRING : in::REP_STATICSTRING),
    flags(f), length(v.size()), data{.as_char_ptr = v.data()}
{
    require(v.size() <= uint32(-1));
    v.unsafe_set_empty();
}
constexpr Tree::Tree (TreeArray v, TreeFlags f) :
    form(Form::Array), rep(in::REP_ARRAY), flags(f),
    length(v.size()), data{.as_array_ptr = v.data()}
{
    require(v.size() <= uint32(-1));
    v.unsafe_set_empty();
}
constexpr Tree::Tree (TreeObject v, TreeFlags f) :
    form(Form::Object), rep(in::REP_OBJECT), flags(f),
    length(v.size()), data{.as_object_ptr = v.data()}
{
    require(v.size() <= uint32(-1));
    v.unsafe_set_empty();
}
inline Tree::Tree (std::exception_ptr v, TreeFlags f) :
    form(Form::Error), rep(in::REP_ERROR), flags(f), length(1), data{}
{
    auto e = SharedArray<std::exception_ptr>(1, move(v));
    const_cast<const std::exception_ptr*&>(data.as_error_ptr) = e.data();
    e.unsafe_set_empty();
}
constexpr Tree::Tree (Tree&& o) :
    form(o.form), rep(o.rep), flags(o.flags), length(o.length), data(o.data)
{
    if (std::is_constant_evaluated()) {
        require(o.rep >= 0);
    }
    else {
         // Optimization.  The member initializers above will be discarded by
         // Dead Store Elimination, and replaced with this memcpy which the
         // compiler optimizes better.
        std::memcpy(this, &o, sizeof(Tree));
        std::memset((void*)&o, 0, sizeof(Tree));
    }
}
constexpr Tree::Tree (const Tree& o) :
    form(o.form), rep(o.rep), flags(o.flags), length(o.length), data(o.data)
{
    if (std::is_constant_evaluated()) {
        require(o.rep >= 0);
    }
    else {
        std::memcpy(this, &o, sizeof(Tree));
        if (rep < 0 && data.as_char_ptr) {
            ++SharableBuffer<char>::header(data.as_char_ptr)->ref_count;
        }
    }
}

constexpr Tree::~Tree () {
    if (rep < 0 && data.as_char_ptr) {
        auto header = SharableBuffer<char>::header(data.as_char_ptr);
        if (header->ref_count) --header->ref_count;
        else in::delete_Tree_data(*this);
    }
}

constexpr Tree::operator Null () const {
    if (rep != in::REP_NULL) in::raise_TreeWrongForm(*this, Form::Null);
    return null;
}
constexpr Tree::operator bool () const {
    if (rep != in::REP_BOOL) in::raise_TreeWrongForm(*this, Form::Bool);
    return data.as_bool;
}
constexpr Tree::operator char () const {
    switch (rep) { \
        case in::REP_STATICSTRING:
        case in::REP_SHAREDSTRING:
            if (length != 1) in::raise_TreeCantRepresent("char", *this);
            return data.as_char_ptr[0];
        default: in::raise_TreeWrongForm(*this, Form::String);
    }
}
#define AYU_INTEGRAL_CONVERSION(T) \
constexpr Tree::operator T () const { \
    switch (rep) { \
        case in::REP_INT64: { \
            int64 v = data.as_int64; \
            if (int64(T(v)) == v) return v; \
            else in::raise_TreeCantRepresent(#T, *this); \
        } \
        case in::REP_DOUBLE: { \
            double v = data.as_double; \
            if (double(T(v)) == v) return v; \
            else in::raise_TreeCantRepresent(#T, *this); \
        } \
        default: in::raise_TreeWrongForm(*this, Form::Number); \
    } \
}
AYU_INTEGRAL_CONVERSION(int8)
AYU_INTEGRAL_CONVERSION(uint8)
AYU_INTEGRAL_CONVERSION(int16)
AYU_INTEGRAL_CONVERSION(uint16)
AYU_INTEGRAL_CONVERSION(int32)
AYU_INTEGRAL_CONVERSION(uint32)
AYU_INTEGRAL_CONVERSION(int64)
AYU_INTEGRAL_CONVERSION(uint64)
#undef AYU_INTEGRAL_CONVERSION
constexpr Tree::operator double () const {
    switch (rep) {
         // Special case: allow null to represent +nan for JSON compatibility
        case in::REP_NULL: return +uni::nan;
        case in::REP_INT64: return data.as_int64;
        case in::REP_DOUBLE: return data.as_double;
        default: in::raise_TreeWrongForm(*this, Form::Number);
    }
}
constexpr Tree::operator Str () const {
    switch (rep) {
        case in::REP_STATICSTRING:
        case in::REP_SHAREDSTRING:
            return Str(data.as_char_ptr, length);
        default: in::raise_TreeWrongForm(*this, Form::String);
    }
}
constexpr Tree::operator AnyString () const& {
    switch (rep) {
        case in::REP_STATICSTRING:
            return StaticString(data.as_char_ptr, length);
        case in::REP_SHAREDSTRING: {
            if (data.as_char_ptr) {
                ++SharableBuffer<char>::header(data.as_char_ptr)->ref_count;
            }
            return SharedString::UnsafeConstructOwned(
                const_cast<char*>(data.as_char_ptr), length
            );
        }
        default: in::raise_TreeWrongForm(*this, Form::String);
    }
}
inline Tree::operator AnyString () && {
    switch (rep) {
        case in::REP_STATICSTRING:
            return StaticString(data.as_char_ptr, length);
        case in::REP_SHAREDSTRING: {
            auto r = SharedString::UnsafeConstructOwned(
                const_cast<char*>(data.as_char_ptr), length
            );
            new (this) Tree();
            return r;
        }
        default: in::raise_TreeWrongForm(*this, Form::String);
    }
}
inline Tree::operator UniqueString16 () const {
    return to_utf16(Str(*this));
}
constexpr Tree::operator TreeArraySlice () const {
    if (rep != in::REP_ARRAY) in::raise_TreeWrongForm(*this, Form::Array);
    return TreeArraySlice(data.as_array_ptr, length);
}
constexpr Tree::operator TreeArray () const& {
    if (rep != in::REP_ARRAY) in::raise_TreeWrongForm(*this, Form::Array);
    if (data.as_array_ptr) {
        ++SharableBuffer<Tree>::header(data.as_array_ptr)->ref_count;
    }
    return TreeArray::UnsafeConstructOwned(
        const_cast<Tree*>(data.as_array_ptr), length
    );
}
inline Tree::operator TreeArray () && {
    if (rep != in::REP_ARRAY) in::raise_TreeWrongForm(*this, Form::Array);
    auto r = TreeArray::UnsafeConstructOwned(
        const_cast<Tree*>(data.as_array_ptr), length
    );
    new (this) Tree();
    return r;
}
constexpr Tree::operator TreeObjectSlice () const {
    if (rep != in::REP_OBJECT) in::raise_TreeWrongForm(*this, Form::Object);
    return TreeObjectSlice(data.as_object_ptr, length);
}
constexpr Tree::operator TreeObject () const& {
    if (rep != in::REP_OBJECT) in::raise_TreeWrongForm(*this, Form::Object);
    if (data.as_object_ptr) {
        ++SharableBuffer<TreePair>::header(data.as_object_ptr)->ref_count;
    }
    return TreeObject::UnsafeConstructOwned(
        const_cast<TreePair*>(data.as_object_ptr), length
    );
}
inline Tree::operator TreeObject () && {
    if (rep != in::REP_OBJECT) in::raise_TreeWrongForm(*this, Form::Object);
    auto r = TreeObject::UnsafeConstructOwned(
        const_cast<TreePair*>(data.as_object_ptr), length
    );
    new (this) Tree();
    return r;
}
inline Tree::operator std::exception_ptr () const {
    if (rep != in::REP_ERROR) in::raise_TreeWrongForm(*this, Form::Error);
    return *data.as_error_ptr;
}

constexpr const Tree* Tree::attr (Str key) const {
    if (rep != in::REP_OBJECT) in::raise_TreeWrongForm(*this, Form::Object);
    for (auto& p : TreeObjectSlice(*this)) {
        if (p.first == key) return &p.second;
    }
    return null;
}
constexpr const Tree* Tree::elem (usize index) const {
    if (rep != in::REP_ARRAY) in::raise_TreeWrongForm(*this, Form::Array);
    auto a = TreeArraySlice(*this);
    if (index < a.size()) return &a[index];
    else return null;
}
constexpr const Tree& Tree::operator[] (Str key) const {
    if (const Tree* r = attr(key)) return *r;
    else raise(e_General, cat(
        "This tree has no attr with key \"", key, '"'
    ));
}
constexpr const Tree& Tree::operator[] (usize index) const {
    if (const Tree* r = elem(index)) return *r;
    else raise(e_General, cat(
        "This tree has no elem with index ", index
    ));
}

} // ayu
