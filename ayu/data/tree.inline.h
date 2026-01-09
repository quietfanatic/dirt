
namespace ayu {
namespace in {

NOINLINE
void delete_Tree_data (Tree&) noexcept;

[[noreturn]]
void raise_TreeWrongForm (const Tree&, Form);
[[noreturn]]
void raise_TreeCantRepresent (StaticString, const Tree&);

static constexpr void check_form (const Tree& self, Form expected) {
    if (self.form != expected) in::raise_TreeWrongForm(self, expected);
}

 // Don't call with s=0!
void check_uniqueness (u32 s, const TreePair* p);

} // in

constexpr Tree::Tree () :
    data{.as_i64 = 0}
{ if (!std::is_constant_evaluated()) {
    std::memset((void*)this, 0, sizeof(Tree));
} }
constexpr Tree::Tree (Null, TreeFlags f) :
    form(Form::Null), flags(f), data{.as_i64 = 0}
{ }
 // Previously we wrote .as_i64 instead of .as_bool, but then we can't access
 // .as_bool at constexpr time because active union members are tracked at
 // constexpr time.
template <class T> requires (std::is_same_v<T, bool>)
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Bool), flags(f), data{.as_bool = v}
{ }
template <class T> requires (
    std::is_integral_v<T> &&
    !std::is_same_v<T, bool> && !std::is_same_v<T, char>
)
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Number), flags(f), data{.as_i64 = i64(v)}
{ }
template <class T> requires (std::is_floating_point_v<T>)
constexpr Tree::Tree (T v, TreeFlags f) :
    form(Form::Number), flags(f), floaty(true), data{.as_double = v}
{ }
constexpr Tree::Tree (AnyString v, TreeFlags f) :
    form(Form::String), flags(f),
    owned(v.owned()), size(v.size()),
    data{.as_char_ptr = v.impl.data}
{
    v.impl = {};
}
constexpr Tree::Tree (AnyArray<Tree> v, TreeFlags f) :
    form(Form::Array), flags(f),
    owned(v.owned()), size(v.size()),
    data{.as_array_ptr = v.impl.data}
{
    v.impl = {};
}
constexpr Tree::Tree (AnyArray<TreePair> v, TreeFlags f) :
    form(Form::Object), flags(f),
    owned(v.owned()), size(v.size()),
    data{.as_object_ptr = v.impl.data}
{
    if (size) {
         // Check for duplicate keys.  Exceptions in constructors do not trigger
         // destructors, so we don't need to clean up our data members.  NOTE:
         // If we move the data members to a subclass then we WILL need to clean
         // them up!
        in::check_uniqueness(size, data.as_object_ptr);
    }
    v.impl = {};
}
inline Tree::Tree (std::exception_ptr v, TreeFlags f) :
    form(Form::Error), flags(f), owned(true), size(1), data{}
{
    auto e = AnyArray<std::exception_ptr>(1, move(v));
    data.as_error_ptr = e.impl.data;
    e.impl = {};
}

constexpr Tree::Tree (Tree&& o) {
    if (std::is_constant_evaluated()) {
        form = o.form; flags = o.flags; floaty = o.floaty; owned = o.owned; size = o.size; data = o.data;
        o.form = Form::Undefined; o.flags = {}; o.floaty = false; o.owned = false; o.size = 0; o.data.as_i64 = 0;
    }
    else {
        std::memcpy((void*)this, &o, sizeof(Tree));
        std::memset((void*)&o, 0, sizeof(Tree));
    }
}
constexpr Tree::Tree (const Tree& o) :
    form(o.form), flags(o.flags), floaty(o.floaty), owned(o.owned), size(o.size), data(o.data)
{
    if (owned) {
        ++SharableBuffer<char>::header(data.as_char_ptr)->ref_count;
    }
}

constexpr Tree& Tree::operator= (Tree&& o) {
    if (owned) [[unlikely]] {
        auto header = SharableBuffer<char>::header(data.as_char_ptr);
        if (!--header->ref_count) in::delete_Tree_data(*this);
    }
    if (std::is_constant_evaluated()) {
        form = o.form; flags = o.flags; floaty = o.floaty; owned = o.owned; size = o.size; data = o.data;
        o.form = Form::Undefined; o.flags = {}; o.floaty = false; o.owned = false; o.size = 0; o.data.as_i64 = 0;
    }
    else {
        std::memcpy((void*)this, &o, sizeof(Tree));
        std::memset((void*)&o, 0, sizeof(Tree));
    }
    return *this;
}
constexpr Tree& Tree::operator= (const Tree& o) {
    if (owned) [[unlikely]] {
        auto header = SharableBuffer<char>::header(data.as_char_ptr);
        if (!--header->ref_count) in::delete_Tree_data(*this);
    }
    form = o.form; flags = o.flags; floaty = o.floaty; owned = o.owned; size = o.size; data = o.data;
    if (owned) {
         // data.as_*_ptr should never be null if the refcounted bit is set
        ++SharableBuffer<char>::header(data.as_char_ptr)->ref_count;
    }
    return *this;
}

constexpr Tree::~Tree () {
    if (owned) [[unlikely]] {
        auto header = SharableBuffer<char>::header(data.as_char_ptr);
        if (!--header->ref_count) in::delete_Tree_data(*this);
    }
}

constexpr Tree::operator Null () const {
    in::check_form(*this, Form::Null);
    return null;
}
constexpr Tree::operator bool () const {
    in::check_form(*this, Form::Bool);
    return data.as_bool;
}
constexpr Tree::operator char () const {
    in::check_form(*this, Form::String);
    if (size != 1) in::raise_TreeCantRepresent("char", *this);
    return data.as_char_ptr[0];
}
#define AYU_INTEGRAL_CONVERSION(T) \
constexpr Tree::operator T () const { \
    in::check_form(*this, Form::Number); \
    if (floaty) { \
        double v = data.as_double; \
        if (double(T(v)) == v) return v; \
        else in::raise_TreeCantRepresent(#T, *this); \
    } \
    else { \
        i64 v = data.as_i64; \
        if (i64(T(v)) == v) return v; \
        else in::raise_TreeCantRepresent(#T, *this); \
    } \
}
AYU_INTEGRAL_CONVERSION(i8)
AYU_INTEGRAL_CONVERSION(u8)
AYU_INTEGRAL_CONVERSION(i16)
AYU_INTEGRAL_CONVERSION(u16)
AYU_INTEGRAL_CONVERSION(i32)
AYU_INTEGRAL_CONVERSION(u32)
AYU_INTEGRAL_CONVERSION(i64)
AYU_INTEGRAL_CONVERSION(u64)
#undef AYU_INTEGRAL_CONVERSION
constexpr Tree::operator double () const {
     // Special case: allow null to represent +nan for JSON compatibility
    if (form == Form::Null) return uni::nan;
    else in::check_form(*this, Form::Number);
    if (floaty) return data.as_double;
    else return data.as_i64;
}
constexpr Tree::operator Str () const {
    in::check_form(*this, Form::String);
    return Str(data.as_char_ptr, size);
}
constexpr Tree::operator AnyString () const& {
    in::check_form(*this, Form::String);
    if (owned) {
        ++SharableBuffer<char>::header(data.as_char_ptr)->ref_count;
    }
    AnyString r;
    r.impl.sizex2_with_owned = (size << 1) | owned;
    r.impl.data = const_cast<char*>(data.as_char_ptr);
    return r;
}
constexpr Tree::operator AnyString () && {
    in::check_form(*this, Form::String);
    AnyString r;
    r.impl.sizex2_with_owned = (size << 1) | owned;
    r.impl.data = const_cast<char*>(data.as_char_ptr);
    form = Form::Undefined; flags = {}; floaty = false; owned = false; size = 0; data.as_char_ptr = null;
    return r;
}
constexpr Tree::operator Slice<Tree> () const {
    in::check_form(*this, Form::Array);
    return Slice<Tree>(data.as_array_ptr, size);
}
constexpr Tree::operator AnyArray<Tree> () const& {
    in::check_form(*this, Form::Array);
    if (owned) {
        ++SharableBuffer<Tree>::header(data.as_array_ptr)->ref_count;
    }
    AnyArray<Tree> r;
    r.impl.sizex2_with_owned = (size << 1) | owned;
    r.impl.data = const_cast<Tree*>(data.as_array_ptr);
    return r;
}
constexpr Tree::operator AnyArray<Tree> () && {
    in::check_form(*this, Form::Array);
    AnyArray<Tree> r;
    r.impl.sizex2_with_owned = (size << 1) | owned;
    r.impl.data = const_cast<Tree*>(data.as_array_ptr);
    form = Form::Undefined; flags = {}; floaty = false; owned = false; size = 0; data.as_array_ptr = null;
    return r;
}
constexpr Tree::operator Slice<TreePair> () const {
    in::check_form(*this, Form::Object);
    return Slice<TreePair>(data.as_object_ptr, size);
}
constexpr Tree::operator AnyArray<TreePair> () const& {
    in::check_form(*this, Form::Object);
    if (owned) {
        ++SharableBuffer<TreePair>::header(data.as_object_ptr)->ref_count;
    }
    AnyArray<TreePair> r;
    r.impl.sizex2_with_owned = (size << 1) | owned;
    r.impl.data = const_cast<TreePair*>(data.as_object_ptr);
    return r;
}
constexpr Tree::operator AnyArray<TreePair> () && {
    in::check_form(*this, Form::Object);
    AnyArray<TreePair> r;
    r.impl.sizex2_with_owned = (size << 1) | owned;
    r.impl.data = const_cast<TreePair*>(data.as_object_ptr);
    form = Form::Undefined; flags = {}; floaty = false; owned = false; size = 0; data.as_object_ptr = null;
    return r;
}
inline Tree::operator std::exception_ptr () const {
    in::check_form(*this, Form::Error);
    return *data.as_error_ptr;
}

constexpr const Tree* Tree::attr (Str key) const {
    in::check_form(*this, Form::Object);
    for (auto& p : Slice<TreePair>(*this)) {
        if (p.first == key) return &p.second;
    }
    return null;
}
constexpr const Tree* Tree::elem (u32 index) const {
    in::check_form(*this, Form::Array);
    auto a = Slice<Tree>(*this);
    if (index < a.size()) return &a[index];
    else return null;
}
constexpr const Tree& Tree::operator[] (Str key) const {
    if (const Tree* r = attr(key)) return *r;
    else raise(e_General, cat(
        "This tree has no attr with key \"", key, '"'
    ));
}
constexpr const Tree& Tree::operator[] (u32 index) const {
    if (const Tree* r = elem(index)) return *r;
    else raise(e_General, cat(
        "This tree has no elem with index ", index
    ));
}

} // ayu
