#include "document.h"

#include "../common.h"
#include "../reflection/describe.h"
#include "../reflection/reference.h"

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
    DocumentData* document;
    AnyString name;
    usize id;
    DocumentItemHeader* header;
    DocumentItemRef (DocumentData* doc, const AnyString& name_) :
        document(doc), name(name_), id(parse_numbered_name(name_))
    {
         // Find item if it exists
        for (auto link = doc->items.next; link != &doc->items; link = link->next) {
            auto h = static_cast<DocumentItemHeader*>(link);
            if (id != usize(-1) ? h->id == id : h->name == name) {
                header = h;
                return;
            }
        }
         // Doesn't exist, we'll autovivify later.
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
        [](ayu::Document& v, const AnyArray<AnyString>&){
             // Setting keys clears the document, but it doesn't actually create
             // the document items.  They aren't created until the type is set
             // on a DocumentItemRef.
            v.data->~DocumentData();
            new (v.data) DocumentData;
        }
    )),
    attr_func([](ayu::Document& v, const AnyString& k){
        if (k == "_next_id") {
            return Reference(&v.data->next_id);
        }
        else {
            auto ref = DocumentItemRef(v.data, k);
            return Reference(
                v, variable(move(ref), pass_through_addressable)
            );
        }
    })
)

AYU_DESCRIBE(ayu::in::DocumentItemRef,
    elems(
        elem(value_funcs<Type>(
            [](const DocumentItemRef& v){
                if (v.header) return v.header->type;
                else return Type();
            },
            [](DocumentItemRef& v, Type t){
                if (!t) raise(e_General, "Document item cannot have no type");
                auto& doc = reinterpret_cast<Document&>(v.document);
                if (v.header) [[unlikely]] {
                     // Item exists?  We'll have to clobber it.  This will
                     // change its order in the document.
                    doc.delete_(v.header->type, v.header->data());
                }
                void* data = doc.allocate_named(t, v.name);
                v.header = (DocumentItemHeader*)data - 1;
                t.default_construct(data);
            }
        )),
        elem(reference_func([](DocumentItemRef& v){
            return Reference(v.header->type, v.header->data());
        }))
    )
)

