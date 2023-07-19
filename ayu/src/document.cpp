#include "../document.h"

#include <cctype>

#include "../common.h"
#include "../describe.h"
#include "../reference.h"

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
    ~DocumentData () {
        while (items.next != &items) {
            auto header = static_cast<DocumentItemHeader*>(items.next);
            if (header->type) header->type.destroy(header->data());
            header->~DocumentItemHeader();
            free(header);
        }
    }
};

struct DocumentItemRef {
    DocumentItemHeader* header;
    DocumentItemRef (DocumentData* doc, Str name) {
        usize id = parse_numbered_name(name);
        for (auto link = doc->items.next; link != &doc->items; link = link->next) {
            auto h = static_cast<DocumentItemHeader*>(link);
            if (id != usize(-1) ? h->id == id : h->name == name) {
                header = h;
                return;
            }
        }
        header = null;
    }
};

} using namespace in;

Document::Document () noexcept : data(new DocumentData) { }
Document::~Document () { delete data; }

void* Document::allocate (Type t) noexcept {
    auto id = data->next_id++;
    auto p = malloc(sizeof(DocumentItemHeader) + (t ? t.cpp_size() : 0));
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
    auto ref = DocumentItemRef(data, name);
    if (ref.header) {
        raise(e_DocumentItemNameDuplicate, move(name));
    }

    if (id == usize(-1)) {
        auto p = malloc(sizeof(DocumentItemHeader) + (t ? t.cpp_size() : 0));
        auto header = new (p) DocumentItemHeader(&data->items, t, move(name));
        return header+1;
    }
    else { // Actually a numbered item
        if (id > data->next_id + 10000) {
            raise(e_General, "Unreasonable growth of _next_id");
        }
        if (id >= data->next_id) data->next_id = id + 1;
        auto p = malloc(sizeof(DocumentItemHeader) + (t ? t.cpp_size() : 0));
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
    free(header);
}

void Document::delete_named (Str name) {
    auto ref = DocumentItemRef(data, name);
    if (ref.header) {
        if (ref.header->type) {
            ref.header->type.destroy(ref.header->data());
        }
        ref.header->~DocumentItemHeader();
        free(ref.header);
        return;
    }
    else raise(e_DocumentItemNotFound, name);
}

void Document::deallocate (void* p) noexcept {
    auto header = (DocumentItemHeader*)p - 1;
    header->~DocumentItemHeader();
    free(header);
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Document,
    keys(mixed_funcs<AnyArray<AnyString>>(
        [](const ayu::Document& v){
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
        [](ayu::Document& v, const AnyArray<AnyString>& ks){
            v.data->~DocumentData();
            new (v.data) DocumentData;
            for (auto& k : ks) {
                if (k == "_next_id") continue;
                v.allocate_named(Type(), k);
            }
        }
    )),
    attr_func([](ayu::Document& v, AnyString k){
        if (k == "_next_id") {
            return Reference(&v.data->next_id);
        }
        else {
            auto ref = DocumentItemRef(v.data, move(k));
            if (ref.header) {
                return Reference(
                    v, variable(move(ref), pass_through_addressable)
                );
            }
            else return Reference();
        }
    })
)

AYU_DESCRIBE(ayu::in::DocumentItemRef,
     // Although nullishness is a valid state for DocumentItemRef (meaning the
     // DocumentItemHeader has no type), we don't want to allow serializing it.
    elems(
        elem(value_funcs<Type>(
            [](const DocumentItemRef& v){
                return v.header->type;
            },
            [](DocumentItemRef& v, Type t){
                if (v.header->type) {
                    v.header->type.destroy(v.header->data());
                }
                 // Using realloc on a C++ object is questionably legal, but
                 // nothing in DocumentItemHeader requires a stable address.
                 // (cast to void* to silence warning)
                 // (note: if instead we call ~DocumentItemHeader without
                 //  first cleaning prev and next, it will reorder items in
                 //  the document.)
                v.header = (DocumentItemHeader*)realloc(
                    (void*)v.header, sizeof(DocumentItemHeader) + (t ? t.cpp_size() : 0)
                );
                v.header->prev->next = v.header;
                v.header->next->prev = v.header;
                v.header->type = t;
                if (v.header->type) {
                    v.header->type.default_construct(v.header->data());
                }
            }
        )),
        elem(reference_func([](DocumentItemRef& v){
            if (v.header->type) {
                return Reference(v.header->type, v.header->data());
            }
            else return Reference();
        }))
    )
)

