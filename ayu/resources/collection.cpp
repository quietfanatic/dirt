#include "collection.h"

#include "../reflection/describe-standard.h"

namespace ayu {

AnyVal* Collection::find_with_name (Str name) noexcept {
    if (!name || !items) [[unlikely]] return null;
    if (name[0] == '_') {
         // Actually a numbered ID
        if (name.size() == 1) [[unlikely]] return null;
        auto [end, id] = read_decimal_digits<u64>(name.begin() + 1, name.end());
        if (end != name.end()) [[unlikely]] return null;
        usize i = last_lookup;
        do {
            if (items[i].id == id && !items[i].name_sx2wo) {
                last_lookup = i;
                return &items[i].value;
            }
            i += 1;
            if (i >= items.size()) i -= items.size();
        }
        while (i != last_lookup);
        return null;
    }
    else {
         // Real name
        usize i = last_lookup;
        do {
            if (items[i].name_sx2wo >> 1 == name.size()) {
                if (items[i].name_data == name.data() ||
                    memeq(items[i].name_data, name.data(), name.size())
                ) {
                    last_lookup = i;
                    return &items[i].value;
                }
            }
            i += 1;
            if (i >= items.size()) i -= items.size();
        }
        while (i != last_lookup);
        return null;
    }
}

void Collection::validate_name (Str name) {
    if (!name) raise(e_CollectionItemNameInvalid, "Collection item name is empty");
    if (name[0] == '_') {
        raise(e_CollectionItemNameInvalid, "Cannot insert item name starting with _");
    }
    for (auto& item : items) {
        if (item.name_sx2wo >> 1 == name.size() &&
            memeq(item.name_data, name.data(), name.size())
        ) raise(e_CollectionItemNameDuplicate, cat("Duplicate item name: ", name));
    }
}

Mu* Collection::extract (Mu* p) noexcept {
    last_lookup = 0; // Don't bother trying to preserve this
    CollectionItem* entry = &items.back(); // debug-asserts nonempty
    items.impl.size -= 1;
     // Explicitly disassemble the objects to dodge destructors and convince the
     // compiler to store everything in registers.  This compiles much smaller
     // than it looks.
    usize tmp_name_sx2wo = entry->name_sx2wo;
    char* tmp_name_data = entry->name_data;
    Type tmp_value_type = entry->value.type;
    Mu* tmp_value_data = entry->value.data;
    while (tmp_value_data != p) {
        entry -= 1;
        expect(entry >= items.begin());

        usize tmp2_name_sx2wo = entry->name_sx2wo;
        char* tmp2_name_data = entry->name_data;
        Type tmp2_value_type = entry->value.type;
        Mu* tmp2_value_data = entry->value.data;

        entry->name_sx2wo = tmp_name_sx2wo;
        entry->name_data = tmp_name_data;
        entry->value.type = tmp_value_type;
        entry->value.data = tmp_value_data;

        tmp_name_sx2wo = tmp2_name_sx2wo;
        tmp_name_data = tmp2_name_data;
        tmp_value_type = tmp2_value_type;
        tmp_value_data = tmp2_value_data;
    }
     // Value has already been deleted, but name needs deleting.
     // Named items are unlikely to ever be deleted.
    if (tmp_name_sx2wo & 1) [[unlikely]] {
        AnyString s;
        s.impl = {tmp_name_sx2wo, tmp_name_data};
    }
    return tmp_value_data;
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Collection,
    keys(mixed_funcs<AnyArray<AnyString>>(
        [](const Collection& v){
            auto r = UniqueArray<AnyString>(Capacity(v.items.size() + 1));
            if (v.next_id) r.emplace_back_expect_capacity("_next_id");
            for (auto& item : v.items) {
                r.emplace_back_expect_capacity(item.name());
            }
            return AnyArray<AnyString>(move(r));
        },
        [](Collection& v, const AnyArray<AnyString>& m){
            auto items = UniqueArray<CollectionItem>(Capacity(m.size()));
            for (auto& k : m) {
                if (!k) raise(e_CollectionItemNameInvalid, "Collection item name is empty");
                if (k[0] == '_') {
                    if (k == "_next_id") continue;
                    auto end = k.end();
                    auto [p, id] = read_decimal_digits<u64>(k.begin() + 1, end);
                    if (p != end) {
                        raise(e_CollectionItemNameInvalid, "Collection item name starts with _ but is not an integer");
                    }
                    items.emplace_back_expect_capacity(id);
                }
                else {
                    items.emplace_back_expect_capacity(k);
                }
            }
            v.items = move(items);
            v.next_id = 0;
            v.last_lookup = 0;
        }
    )),
    computed_attrs([](Collection& v, const AnyString& key)->Link{
        if (key == "_next_id") {
            return Link(&v.next_id);
        }
        else {
            return Link(v.find_with_name(key));
        }
    })
)

