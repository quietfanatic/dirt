
namespace ayu {
namespace in {

NOINLINE
void delete_Tree_data (TreeRef) noexcept;

[[noreturn]]
void raise_TreeWrongForm (TreeRef, Form);
[[noreturn]]
void raise_TreeCantRepresent (StaticString, TreeRef);

} // in

constexpr Tree::Tree () :
    form(Form::Undefined), flags(), meta(0), data{.as_int64 = 0}
{ }
constexpr Tree::Tree (Null, TreeFlags f) :
    form(Form::Null), flags(f), meta(0), data{.as_int64 = 0}
{ }
 // Use .as_int64 to write all of data
template <class T> requires (std::is_same_v<T, bool>)
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Bool), flags(f), meta(0), data{.as_bool = v}
{ }
template <class T> requires (
    std::is_integral_v<T> &&
    !std::is_same_v<T, bool> && !std::is_same_v<T, char>
)
 // Set meta to an even value because we use meta & 1 to see if we need
 // refcounting.
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Number), flags(f),
    meta(0), data{.as_int64 = int64(v)}
{ }
template <class T> requires (std::is_floating_point_v<T>)
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Number), flags(f),
    meta(2), data{.as_double = v}
{ }
constexpr Tree::Tree (AnyString v, TreeFlags f) :
    form(Form::String), flags(f),
    meta(v.impl.sizex2_with_owned), data{.as_char_ptr = v.impl.data}
{
    v.impl = {};
}
constexpr Tree::Tree (AnyArray<Tree> v, TreeFlags f) :
    form(Form::Array), flags(f),
    meta(v.impl.sizex2_with_owned), data{.as_array_ptr = v.impl.data}
{
    v.impl = {};
}
constexpr Tree::Tree (AnyArray<TreePair> v, TreeFlags f) :
    form(Form::Object), flags(f),
    meta(v.impl.sizex2_with_owned), data{.as_object_ptr = v.impl.data}
{
#ifndef NDEBUG
     // Check for duplicate keys
    for (usize i = 0; i < v.size(); i++)
    for (usize j = 0; j < i; j++) {
        expect(v[i].first != v[j].first);
    }
#endif
    v.impl = {};
}
inline Tree::Tree (std::exception_ptr v, TreeFlags f) :
    form(Form::Error), flags(f), meta(), data{}
{
    auto e = AnyArray<std::exception_ptr>(1, move(v));
    meta = e.impl.sizex2_with_owned;
    data.as_error_ptr = e.impl.data;
    e.impl = {};
}

constexpr Tree::Tree (Tree&& o) :
    form(o.form), flags(o.flags), meta(o.meta), data(o.data)
{
    o.meta = 0; o.data.as_int64 = 0;
}
constexpr Tree::Tree (const Tree& o) :
    form(o.form), flags(o.flags), meta(o.meta), data(o.data)
{
    if (meta & 1 && data.as_char_ptr) {
        ++SharableBuffer<char>::header(data.as_char_ptr)->ref_count;
    }
}

constexpr Tree& Tree::operator= (Tree&& o) {
    this->~Tree();
    form = o.form; flags = o.flags; meta = o.meta; data = o.data;
    o.meta = 0; o.data.as_int64 = 0;
    return *this;
}
constexpr Tree& Tree::operator= (const Tree& o) {
    this->~Tree();
    form = o.form; flags = o.flags; meta = o.meta; data = o.data;
    if (meta & 1 && data.as_char_ptr) {
        ++SharableBuffer<char>::header(data.as_char_ptr)->ref_count;
    }
    return *this;
}

constexpr Tree::~Tree () {
    if (meta & 1 && data.as_char_ptr) {
        auto header = SharableBuffer<char>::header(data.as_char_ptr);
        if (!--header->ref_count) in::delete_Tree_data(*this);
    }
}

constexpr Tree::operator Null () const {
    if (form == Form::Null) return null;
    else in::raise_TreeWrongForm(*this, Form::Null);
}
constexpr Tree::operator bool () const {
    if (form == Form::Bool) return data.as_bool;
    else in::raise_TreeWrongForm(*this, Form::Bool);
}
constexpr Tree::operator char () const {
    if (form == Form::String) {
        if (meta >> 1 != 1) in::raise_TreeCantRepresent("char", *this);
        return data.as_char_ptr[0];
    }
    else in::raise_TreeWrongForm(*this, Form::String);
}
#define AYU_INTEGRAL_CONVERSION(T) \
constexpr Tree::operator T () const { \
    if (form == Form::Number) { \
        if (meta) { \
            double v = data.as_double; \
            if (double(T(v)) == v) return v; \
            else in::raise_TreeCantRepresent(#T, *this); \
        } \
        else { \
            int64 v = data.as_int64; \
            if (int64(T(v)) == v) return v; \
            else in::raise_TreeCantRepresent(#T, *this); \
        } \
    } \
    else in::raise_TreeWrongForm(*this, Form::Number); \
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
     // Special case: allow null to represent +nan for JSON compatibility
    if (form == Form::Null) return uni::nan;
    else if (form == Form::Number) {
        if (meta) return data.as_double;
        else return data.as_int64;
    }
    else in::raise_TreeWrongForm(*this, Form::Number);
}
constexpr Tree::operator Str () const {
    if (form == Form::String) {
        return Str(data.as_char_ptr, meta >> 1);
    }
    else in::raise_TreeWrongForm(*this, Form::String);
}
constexpr Tree::operator AnyString () const& {
    if (form == Form::String) {
        if (meta & 1) {
            ++SharableBuffer<char>::header(data.as_char_ptr)->ref_count;
        }
        AnyString r;
        r.impl.sizex2_with_owned = meta;
        r.impl.data = const_cast<char*>(data.as_char_ptr);
        return r;
    }
    else in::raise_TreeWrongForm(*this, Form::String);
}
constexpr Tree::operator AnyString () && {
    if (form == Form::String) {
        AnyString r;
        r.impl.sizex2_with_owned = meta;
        r.impl.data = const_cast<char*>(data.as_char_ptr);
        meta = 0; data.as_char_ptr = null;
        return r;
    }
    else in::raise_TreeWrongForm(*this, Form::String);
}
inline Tree::operator UniqueString16 () const {
    return to_utf16(Str(*this));
}
constexpr Tree::operator Slice<Tree> () const {
    if (form == Form::Array) {
        return Slice<Tree>(data.as_array_ptr, meta >> 1);
    }
    else in::raise_TreeWrongForm(*this, Form::Array);
}
constexpr Tree::operator AnyArray<Tree> () const& {
    if (form == Form::Array) {
        if (meta & 1) {
            ++SharableBuffer<Tree>::header(data.as_array_ptr)->ref_count;
        }
        AnyArray<Tree> r;
        r.impl.sizex2_with_owned = meta;
        r.impl.data = const_cast<Tree*>(data.as_array_ptr);
        return r;
    }
    else in::raise_TreeWrongForm(*this, Form::Array);
}
constexpr Tree::operator AnyArray<Tree> () && {
    if (form == Form::Array) {
        AnyArray<Tree> r;
        r.impl.sizex2_with_owned = meta;
        r.impl.data = const_cast<Tree*>(data.as_array_ptr);
        meta = 0; data.as_array_ptr = null;
        return r;
    }
    else in::raise_TreeWrongForm(*this, Form::Array);
}
constexpr Tree::operator Slice<TreePair> () const {
    if (form == Form::Object) {
        return Slice<TreePair>(data.as_object_ptr, meta >> 1);
    }
    else in::raise_TreeWrongForm(*this, Form::Object);
}
constexpr Tree::operator AnyArray<TreePair> () const& {
    if (form == Form::Object) {
        if (meta & 1) {
            ++SharableBuffer<TreePair>::header(data.as_object_ptr)->ref_count;
        }
        AnyArray<TreePair> r;
        r.impl.sizex2_with_owned = meta;
        r.impl.data = const_cast<TreePair*>(data.as_object_ptr);
        return r;
    }
    else in::raise_TreeWrongForm(*this, Form::Object);
}
constexpr Tree::operator AnyArray<TreePair> () && {
    if (form == Form::Object) {
        AnyArray<TreePair> r;
        r.impl.sizex2_with_owned = meta;
        r.impl.data = const_cast<TreePair*>(data.as_object_ptr);
        meta = 0; data.as_object_ptr = null;
        return r;
    }
    else in::raise_TreeWrongForm(*this, Form::Object);
}
inline Tree::operator std::exception_ptr () const {
    if (form == Form::Error) {
        return *data.as_error_ptr;
    }
    else in::raise_TreeWrongForm(*this, Form::Error);
}

constexpr const Tree* Tree::attr (Str key) const {
    if (form == Form::Object) {
        for (auto& p : Slice<TreePair>(*this)) {
            if (p.first == key) return &p.second;
        }
        return null;
    }
    else in::raise_TreeWrongForm(*this, Form::Object);
}
constexpr const Tree* Tree::elem (usize index) const {
    if (form == Form::Array) {
        auto a = Slice<Tree>(*this);
        if (index < a.size()) return &a[index];
        else return null;
    }
    else in::raise_TreeWrongForm(*this, Form::Array);
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
