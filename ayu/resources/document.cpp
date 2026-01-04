#include "document.h"
#include "../../uni/lilac.h"
#include "../common.h"
#include "../reflection/anyref.h"
#include "../reflection/describe.h"

namespace ayu {
namespace in {

struct DocumentItemHeader : DocumentLinks {
     // TODO: merge these to save one word
    u64 id = -1;
    AnyString name;
    Type type;
    DocumentItemHeader (DocumentLinks* links, Type t, u64 id) :
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
    AnyPtr item () {
        return AnyPtr(type, (Mu*)(this + 1));
    }
};

DocumentData::~DocumentData () {
    while (items.prev != &items) {
        auto header = static_cast<DocumentItemHeader*>(items.prev);
        if (header->type) dynamic_destroy(header->type, header->data());
        header->~DocumentItemHeader();
        lilac::deallocate_unknown_size(header);
    }
}

} using namespace in;

AnyPtr Document::find_with_name (Str name) const {
    if (!name) [[unlikely]] return null;
    if (name[0] == '_') {
        if (name.size() == 1) [[unlikely]] return null;
        auto [p, id] = read_decimal_digits<u64>(name.begin() + 1, name.end());
        if (p != name.end()) [[unlikely]] return null;
        for (auto link = last_lookup->next;; link = link->next) {
            if (link != &items) {
                auto h = static_cast<DocumentItemHeader*>(link);
                if (h->id == id) {
                    last_lookup = link;
                    return h->item();
                }
            }
            if (link == last_lookup) break;
        }
        return null;
    }
    else for (auto link = last_lookup->next;; link = link->next) {
        if (link != &items) {
            auto h = static_cast<DocumentItemHeader*>(link);
            if (h->name == name) {
                last_lookup = link;
                return h->item();
            }
        }
        if (link == last_lookup) break;
    }
    return null;
}

void* Document::allocate (Type t) noexcept {
    expect(t.cpp_align() <= 8);
    auto id = next_id++;
    auto p = lilac::allocate(
        sizeof(DocumentItemHeader) + t.cpp_size()
    );
    auto header = new (p) DocumentItemHeader(&items, t, id);
    return header+1;
}

void* Document::allocate_named (Type t, AnyString name) {
    expect(t.cpp_align() <= 8);
    if (find_with_name(name)) {
        raise(e_DocumentItemNameDuplicate, move(name));
    }
    if (!name) {
        raise(e_DocumentItemNameInvalid, "Document item name cannot be empty");
    }
    if (name[0] == '_') {
         // Actually a numbered item (hopefully)
        if (name.size() == 1) {
            bad_: raise(e_DocumentItemNameInvalid, "Document item name cannot start with _");
        }
        auto [p, id] = read_decimal_digits<u64>(name.begin() + 1, name.end());
        if (p != name.end()) goto bad_;
        if (id == next_id) next_id++;
        auto space = lilac::allocate(
            sizeof(DocumentItemHeader) + t.cpp_size()
        );
        auto header = new (space) DocumentItemHeader(&items, t, id);
        return header+1;
    }
    else {
         // Actual string name
        auto space = lilac::allocate(
            sizeof(DocumentItemHeader) + t.cpp_size()
        );
        auto header = new (space) DocumentItemHeader(&items, t, move(name));
        return header+1;
    }
}

NOINLINE
void Document::delete_ (Type t, Mu* p) noexcept {
#ifndef NDEBUG
     // Check that the pointer belongs to this document
    for (auto link = items.next; link != &items; link = link->next) {
        auto header = static_cast<DocumentItemHeader*>(link);
        if (header->data() == p) goto we_good;
    }
    never();
    we_good:;
#endif
    auto header = (DocumentItemHeader*)p - 1;
    expect(header->type == t);
    if (header->type) dynamic_destroy(header->type, p);
    header->~DocumentItemHeader();
    if (last_lookup == header) last_lookup = &items;
    lilac::deallocate_unknown_size(header);
}

void Document::delete_named (Str name) {
    AnyPtr p = find_with_name(name);
    if (!p) raise(e_DocumentItemNotFound, name);
    delete_(p.type(), p.address);
}

void Document::deallocate (void* p) noexcept {
    auto header = (DocumentItemHeader*)p - 1;
    header->~DocumentItemHeader();
    if (last_lookup == header) last_lookup = &items;
    lilac::deallocate_unknown_size(header);
}

static void Document_before_from_tree (Document& v, const Tree& t) {
     // Each of these checked cases should reliably cause an error later
    if (t.form != Form::Object) [[unlikely]] return;
    auto o = Slice<TreePair>(t);
    v = {};
    for (auto& [key, value] : o) {
        if (value.form != Form::Array) continue;
        auto a = Slice<Tree>(value);
        if (a.size() != 2 || a[0].form != Form::String) continue;
        Type t = Type(Str(a[0]));
        void* p = v.allocate_named(t, key);
        dynamic_default_construct(t, p);
    }
}

static AnyArray<AnyString> Document_get_keys (const Document& v) {
    AnyArray<AnyString> r;
    for (auto link = v.items.next; link != &v.items; link = link->next) {
        auto header = static_cast<DocumentItemHeader*>(link);
        r.emplace_back(header->name
            ? header->name
            : AnyString(cat('_', header->id))
        );
    }
    r.emplace_back("_next_id");
    return r;
}
static void Document_set_keys (Document&, const AnyArray<AnyString>&) {
     // Noop.  The current way Documents work, they don't support calling
     // item_set_keys() followed by item_attr().write(), because they need the
     // types of their items before they can allocate them.  TODO to investigate
     // if there's a way to support that scenario.
}
static AnyRef Document_computed_attrs (Document& v, const AnyString& k) {
    if (k == "_next_id") {
        return AnyRef(&v.next_id);
    }
    else {
        AnyPtr p = v.find_with_name(k);
        if (!p) return AnyRef();
        return AnyRef((DocumentItemHeader*)p.address - 1);
    }
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Document,
    before_from_tree(&Document_before_from_tree),
    keys(mixed_funcs<AnyArray<AnyString>>(
        &Document_get_keys, &Document_set_keys
    )),
    computed_attrs(&Document_computed_attrs)
)

AYU_DESCRIBE(ayu::in::DocumentItemHeader,
    elems(
        elem(value_funcs<Type>(
            [](const DocumentItemHeader& v){
                return v.type;
            },
            [](DocumentItemHeader& v, Type t){
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

