#include "document.h"

#include "../../uni/lilac.h"
#include "../common.h"
#include "../reflection/anyref.h"
#include "../reflection/describe.h"

namespace ayu {
namespace in {

static usize parse_numbered_name (Str name) {
    if (name.empty() || name[0] != '_') return -1;
    usize id;
    auto [ptr, ec] = std::from_chars(name.begin() + 1, name.end(), id);
    if (ptr == name.end()) return id;
    else return -1;
}

struct DocumentLinks {
    DocumentLinks* prev;
    DocumentLinks* next;
    DocumentLinks () : prev(this), next(this) { }
    DocumentLinks (DocumentLinks* o) :
        prev(o->prev),
        next(o)
    {
        o->prev->next = this;
        o->prev = this;
    }
    ~DocumentLinks () {
        prev->next = next;
        next->prev = prev;
    }
};

 // This alignas shouldn't be necessary but just in case.
struct alignas(std::max_align_t) DocumentItemHeader : DocumentLinks {
     // TODO: u32
    usize id = 0;
    AnyString name;
    Type type;
    DocumentItemHeader (DocumentLinks* links, Type t, usize id) :
        DocumentLinks(links),
        id(id),
        name(""),
        type(t)
    { }
    DocumentItemHeader (DocumentLinks* links, Type t, AnyString name) :
        DocumentLinks(links),
        id(-1),
        name(move(name)),
        type(t)
    { }
    Mu* data () {
        return (Mu*)(this + 1);
    }
};

struct DocumentData {
    DocumentLinks items;
    usize next_id = 0;
     // Lookups are likely to be in order, so start searching where the last
     // search ended.
    DocumentLinks* last_lookup = &items;
    ~DocumentData () {
        while (items.next != &items) {
            auto header = static_cast<DocumentItemHeader*>(items.next);
            if (header->type) header->type.destroy(header->data());
            header->~DocumentItemHeader();
            lilac::deallocate_unknown_size(header);
        }
    }
};

} using namespace in;

Document::Document () noexcept : data(new DocumentData) { }
Document::~Document () { delete data; }

AnyPtr Document::find_with_name (Str name) const {
    usize id = parse_numbered_name(name);
    if (id != usize(-1)) return find_with_id(id);
    for (auto link = data->last_lookup->next;; link = link->next) {
        if (link != &data->items) {
            auto h = static_cast<DocumentItemHeader*>(link);
            if (h->id == usize(-1) && h->name == name) {
                data->last_lookup = link;
                return AnyPtr(h->type, h->data());
            }
        }
        if (link == data->last_lookup) break;
    }
    return null;
}
AnyPtr Document::find_with_id (usize id) const {
    expect(id != usize(-1));
    for (auto link = data->last_lookup->next;; link = link->next) {
        if (link != &data->items) {
            auto h = static_cast<DocumentItemHeader*>(link);
            if (h->id == id) {
                data->last_lookup = link;
                return AnyPtr(h->type, h->data());
            }
        }
        if (link == data->last_lookup) break;
    }
    return null;
}

void* Document::allocate (Type t) noexcept {
    auto id = data->next_id++;
    auto p = lilac::allocate(
        sizeof(DocumentItemHeader) + (t ? t.cpp_size() : 0)
    );
    auto header = new (p) DocumentItemHeader(&data->items, t, id);
    return header+1;
}

void* Document::allocate_named (Type t, AnyString name) {
    if (!name) {
        raise(e_DocumentItemNameInvalid, "Empty string");
    }
    usize id = parse_numbered_name(name);
    if (id == usize(-1) && name[0] == '_') {
        raise(e_DocumentItemNameInvalid,
            cat("Names starting with _ are reserved: ", name)
        );
    }

    if (id == usize(-1)) {
        if (find_with_name(name)) {
            raise(e_DocumentItemNameDuplicate, move(name));
        }
        auto p = lilac::allocate(
            sizeof(DocumentItemHeader) + (t ? t.cpp_size() : 0)
        );
        auto header = new (p) DocumentItemHeader(&data->items, t, move(name));
        return header+1;
    }
    else { // Actually a numbered item
        if (find_with_id(id)) {
            raise(e_DocumentItemNameDuplicate, move(name));
        }
        if (id > data->next_id + 10000) {
            raise(e_General, "Unreasonable growth of _next_id");
        }
        if (id >= data->next_id) data->next_id = id + 1;
        auto p = lilac::allocate(
            sizeof(DocumentItemHeader) + (t ? t.cpp_size() : 0)
        );
        auto header = new (p) DocumentItemHeader(&data->items, t, id);
        return header+1;
    }
}

void Document::delete_ (Type t, Mu* p) noexcept {
#ifndef NDEBUG
     // Check that the pointer belongs to this document
    for (auto link = data->items.next; link != &data->items; link = link->next) {
        auto header = static_cast<DocumentItemHeader*>(link);
        if (header->data() == p) goto we_good;
    }
    never();
    we_good:;
#endif
    auto header = (DocumentItemHeader*)p - 1;
    expect(header->type == t);
    if (header->type) header->type.destroy(p);
    header->~DocumentItemHeader();
    if (data->last_lookup == header) data->last_lookup = &data->items;
    lilac::deallocate_unknown_size(header);
}

void Document::delete_named (Str name) {
    AnyPtr p = find_with_name(name);
    if (!p) raise(e_DocumentItemNotFound, name);
    delete_(p.type, p.address);
}

void Document::deallocate (void* p) noexcept {
    auto header = (DocumentItemHeader*)p - 1;
    header->~DocumentItemHeader();
    if (data->last_lookup == header) data->last_lookup = &data->items;
    lilac::deallocate_unknown_size(header);
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Document,
    before_from_tree([](Document& v, const Tree& t){
         // Each of these checked cases should reliably cause an error later
        if (t.form != Form::Object) return;
        auto o = Slice<TreePair>(t);
        v = {};
        for (auto& [key, value] : o) {
            if (value.form != Form::Array) continue;
            auto a = Slice<Tree>(value);
            if (a.size() != 2 || a[0].form != Form::String) continue;
            Type t = Type(Str(a[0]));
            void* p = v.allocate_named(t, key);
            t.default_construct(p);
        }
    }),
    keys(mixed_funcs<AnyArray<AnyString>>(
        [](const Document& v){
            UniqueArray<AnyString> r;
            for (auto link = v.data->items.next; link != &v.data->items; link = link->next) {
                auto header = static_cast<DocumentItemHeader*>(link);
                r.emplace_back(header->id != usize(-1)
                    ? AnyString(cat('_', header->id))
                    : header->name
                );
            }
            r.emplace_back("_next_id");
            return AnyArray(r);
        },
        [](Document&, const AnyArray<AnyString>&){
             // Noop.  TODO: do some validation?
        }
    )),
    computed_attrs([](Document& v, const AnyString& k){
        if (k == "_next_id") {
            return AnyRef(&v.data->next_id);
        }
        else {
            AnyPtr p = v.find_with_name(k);
            if (!p) return AnyRef();
            return AnyRef((DocumentItemHeader*)p.address - 1);
        }
    })
)

AYU_DESCRIBE(ayu::in::DocumentItemHeader,
    elems(
        elem(value_funcs<Type>(
            [](const DocumentItemHeader& v){
                return v.type;
            },
            [](DocumentItemHeader& v, Type t){
                if (!t) raise(e_General, "Document item cannot have no type");
                if (t != v.type) raise(e_General,
                    "Cannot set a document item's type outside of a from_tree operation."
                );
            }
        )),
        elem(anyptr_func([](DocumentItemHeader& v){
            return AnyPtr(v.type, v.data());
        }))
    )
)

