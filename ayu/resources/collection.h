#pragma once
#include "../common.h"
#include "../data/tree.h"
#include "../reflection/anyval.h"
#include "../reflection/type.h"

namespace ayu {

// This is an owning container for dynamic values with optional names, intended
// to be the top-level item of a resource.  You can think of it like an
// unordered_map<AnyString, AnyVal>, except that order is preserved, and
// insertion and deletion are prioritized over lookup.
//
// Anonymous items are assigned a sequential integer id.  This can be referred
// to like it's a name by putting _ before the decimal representation of the
// number, like "_36".  These pseudonyms are stored as integers, not strings, so
// anonymous items are cheaper than named items.  Other names starting with _
// are forbidden.
//
// Item types cannot have an alignment larger than 8.

struct CollectionItem;

struct Collection {

    Collection () { }
    Collection (Collection&& o) = default;
    Collection (const Collection&) = delete;
    Collection& operator= (Collection&& o) = default;
    Collection& operator= (const Collection& o) = delete;
    ~Collection () { }

     // Emplace a new anonymous item into this collection.  O(1)
    template <Describable T, class... Args>
    T* new_ (Args&&... args);

     // Delete an item in this collection.  O(n) in the general case but starts
     // from the back, so deleting recently newed items is O(1) for a fixed
     // definition of "recently".  UB with debug-assert if the item is not in
     // this collection or p is null.
    template <Describable T>
    void delete_ (T*);

    void delete_ (AnyPtr);

     // Emplace a new named item into this collection.  O(n) to make sure the
     // name is unique.  The name cannot be an anonymous ID.
    template <Describable T, class... Args>
    T* new_with_name (AnyString name, Args&&... args);
     // Like above, but O(1) and undefined behavior if name isn't unique and a
     // valid item name.
    template <Describable T, class... Args>
    T* new_with_name_expect_valid (AnyString name, Args&&... args);

     // Returns the item with the given name.  Can find anonymous items if you
     // pass a decimal integer prefixed with _.  Returns null if not found or if
     // the name is invalid (starts with _ but is not an anonymous ID).  This is
     // typically O(n), but may be O(1) if you find the same item twice in a
     // row or iterate over items sequentially.
    AnyVal* find_with_name (Str) noexcept;

    UniqueArray<CollectionItem> items;
    u64 next_id = 0;
    u32 last_lookup = 0;

private:
    void validate_name (Str);
    Mu* extract (Mu*) noexcept;
};

 // Tried to create a Collection item with an invalid name (starting with a _).
constexpr ErrorCode e_CollectionItemNameInvalid = "ayu::e_CollectionItemNameInvalid";
 // Tried to create a Collection item with a name that's already in use in
 // this document.
constexpr ErrorCode e_CollectionItemNameDuplicate = "ayu::e_CollectionItemNameDuplicate";

///// INLINES

struct CollectionItem {
    usize name_sx2wo;
    union {
        u64 id;
        char* name_data;
    };
    AnyVal value;

    AnyString name () const noexcept {
        if (name_sx2wo) {
            AnyString tmp;
            tmp.impl = {name_sx2wo, name_data};
            AnyString r = tmp; // Trigger refcount
            tmp.impl = {};
            return r;
        }
        else return cat('_', id);
    }

    CollectionItem (u64 id, AnyVal&& v = {}) :
        name_sx2wo(0), id(id),
        value(move(v))
    { }

    CollectionItem (AnyString n, AnyVal&& v = {}) :
        name_sx2wo(n.impl.sizex2_with_owned),
        name_data(n.impl.data),
        value(move(v))
    { n.impl = {}; }

    CollectionItem (CollectionItem&& o) :
        name_sx2wo(o.name_sx2wo),
        name_data(o.name_data),
        value(move(o.value))
    { o.name_sx2wo = 0; }

    CollectionItem& operator= (CollectionItem&& o) {
        this->~CollectionItem();
        new (this) CollectionItem(move(o));
        return *this;
    }

    ~CollectionItem () {
        if (name_sx2wo & 1) {
            AnyString s;
            s.impl = {name_sx2wo, name_data};
        }
    }
};

template <Describable T, class... Args>
T* Collection::new_ (Args&&... args) {
    auto p = new T (std::forward<Args>(args)...);
    auto& item = items.emplace_back(
        next_id++, AnyVal(Type::of<T>(), (Mu*)p)
    );
    return (T*)item.value.data;
}

template <Describable T>
void Collection::delete_ (T* p) {
    auto p2 = extract((Mu*)p);
    delete (T*)p2;
}

inline void Collection::delete_ (AnyPtr p) {
    auto p2 = extract(p.address);
    dynamic_delete(p.type(), p2);
}

template <Describable T, class... Args>
T* Collection::new_with_name (AnyString name, Args&&... args) {
    validate_name(name);
    auto p = new T (std::forward<Args>(args)...);
    auto& item = items.emplace_back(
        move(name), AnyVal(Type::of<T>(), (Mu*)p)
    );
    return (T*)item.value.data;
}

template <Describable T, class... Args>
T* Collection::new_with_name_expect_valid (AnyString name, Args&&... args) {
#ifndef NDEBUG
    validate_name(name);
#endif
    auto p = new T (std::forward<Args>(args)...);
    auto& item = items.emplace_back(
        move(name), AnyVal(Type::of<T>(), (Mu*)p)
    );
    return (T*)item.value.data;
}

} // namespace ayu

template <>
struct uni::IsTriviallyRelocatableS<ayu::CollectionItem> {
    static constexpr bool value = true;
};
