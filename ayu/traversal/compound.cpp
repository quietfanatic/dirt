#include "compound.h"

#include "traversal.private.h"
#include "../reflection/descriptors.private.h"

namespace ayu {
namespace in {

 // Pulling out this callback to avoid redundant instantiations of
 // lambdas due to all lambdas having unique types.
struct ReceiveReference {
    Reference& r;
    void operator() (const Traversal& child) const {
        new (&r) Reference(child.to_reference());
    }
};

///// GET KEYS

struct TraverseGetKeys {
    UniqueArray<AnyString> keys;

    NOINLINE
    void start (const Reference& item, LocationRef loc) {
        trav_start(item, loc, false, AccessMode::Read,
            [this](const Traversal& trav)
        { traverse(trav); });
    }

    void collect (AnyString&& key) {
         // This'll end up being N^2.  TODO: Test whether including an unordered_set
         // would speed this up (probably not).  Maybe even just hashing the key
         // might be enough.
        for (auto k : keys) if (k == key) return;
        keys.emplace_back(move(key));
    }

    NOINLINE void traverse (const Traversal& trav) {
        if (auto acr = trav.desc->keys_acr()) {
            use_computed_attrs(trav, acr);
        }
        else if (auto attrs = trav.desc->attrs()) {
            use_attrs(trav, attrs);
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(trav, acr);
        }
        else raise_AttrsNotSupported(trav.desc);
    }

    NOINLINE void use_attrs (
        const Traversal& trav, const AttrsDcrPrivate* attrs
    ) {
        for (uint16 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (!!(acr->attr_flags & AttrFlags::Invisible)) continue;
            if (!!(acr->attr_flags & AttrFlags::Include)) {
                trav_attr(trav, acr, attr->key, AccessMode::Read,
                    [this](const Traversal& child)
                { traverse(child); });
            }
            else collect(attr->key);
        }
    }

    void use_computed_attrs (
        const Traversal& trav, const Accessor* keys_acr
    ) {
        keys_acr->read(*trav.address,
            CallbackRef<void(Mu&)>(*this, [](auto& self, Mu& v)
        {
            auto& item_keys = reinterpret_cast<const AnyArray<AnyString>&>(v);
            for (auto& key : item_keys) {
                self.collect(AnyString(key));
            }
        }));
    }

    NOINLINE void use_delegate (
        const Traversal& trav, const Accessor* acr
    ) {
        trav_delegate(trav, acr, AccessMode::Read,
            [this](const Traversal& child)
        { traverse(child); });
    }
};

} using namespace in;

AnyArray<AnyString> item_get_keys (
    const Reference& item, LocationRef loc
) {
    UniqueArray<AnyString> keys;
    reinterpret_cast<TraverseGetKeys&>(keys).start(item, loc);
    return keys;
}

///// SET KEYS

namespace in {

struct TraverseSetKeys {
    UniqueArray<AnyString> keys;

    NOINLINE
    void start (const Reference& item, LocationRef loc) {
        trav_start(item, loc, false, AccessMode::Read,
            [this](const Traversal& trav)
        {
            traverse(trav);
            if (keys) raise_AttrRejected(trav.desc, keys[0]);
        });
    }

    bool claim (Str key) {
         // This algorithm overall is O(N^3), we may be able to speed it up by
         // setting a flag if there are no included attrs, or maybe by using an
         // unordered_set?
         // TODO: Use a next_list like in from-tree.
        for (usize i = 0; i < keys.size(); ++i) {
            if (keys[i] == key) {
                keys.erase(i);
                return true;
            }
        }
        return false;
    }

    NOINLINE void traverse (const Traversal& trav) {
        if (auto acr = trav.desc->keys_acr()) {
            if (!(acr->flags & AcrFlags::Readonly)) {
                use_computed_attrs(trav, acr);
            }
            else {
                use_computed_attrs_readonly(trav, acr);
            }
        }
        else if (auto attrs = trav.desc->attrs()) {
            use_attrs(trav, attrs);
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(trav, acr);
        }
        else raise_AttrsNotSupported(trav.desc);
    }

    NOINLINE void use_attrs (
        const Traversal& trav, const AttrsDcrPrivate* attrs
    ) {
         // Prioritize direct attrs
         // I don't think it's possible for n_attrs to be large enough to
         // overflow the stack...right?  The max description size is 64K and an
         // attr always consumes at least 14 bytes, so the max n_attrs is
         // something like 4500.  TODO: enforce a reasonable max n_attrs in
         // descriptors-internal.h.
        bool claimed [attrs->n_attrs] = {};
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (claim(attr->key)) {
                claimed[i] = true;
            }
            else if (!!(acr->attr_flags &
                (AttrFlags::Optional|AttrFlags::Include)
            )) {
                 // Allow omitting optional or included attrs
            }
            else raise_AttrMissing(trav.desc, attr->key);
        }
         // Then check included attrs
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (!!(acr->attr_flags & AttrFlags::Include)) {
                 // Skip if attribute was given directly, uncollapsed
                if (claimed[i]) continue;
                trav_attr(trav, acr, attr->key, AccessMode::Write,
                    [this](const Traversal& child)
                { traverse(child); });
            }
        }
    }

    void use_computed_attrs (
        const Traversal& trav, const Accessor* keys_acr
    ) {
        keys_acr->write(*trav.address,
            CallbackRef<void(Mu&)>(move(keys), [](auto&& keys, Mu& v){
                reinterpret_cast<AnyArray<AnyString>&>(v) = move(keys);
            })
        );
    }
    NOINLINE void use_computed_attrs_readonly (
        const Traversal& trav, const Accessor*
    ) {
         // For readonly keys, get the keys and compare them.
         // TODO: This can probably be optimized more
        TraverseGetKeys tgk;
        tgk.traverse(trav);
        for (auto& key : tgk.keys) {
            if (!claim(key)) {
                raise_AttrMissing(trav.desc, key);
            }
        }
    }

    NOINLINE void use_delegate (
        const Traversal& trav, const Accessor* acr
    ) {
        trav_delegate(trav, acr, AccessMode::Write,
            [this](const Traversal& child)
        { traverse(child); });
    }

};

} using namespace in;

void item_set_keys (
    const Reference& item, AnyArray<AnyString> keys, LocationRef loc
) {
    TraverseSetKeys(move(keys)).start(item, loc);
}

///// ATTR

struct TraverseAttr {
    NOINLINE static
    Reference start (
        const Reference& item, const AnyString& key, LocationRef loc
    ) {
        Reference r;
        trav_start(item, loc, false, AccessMode::Read,
            [&r, &key](const Traversal& trav)
        { traverse(r, trav, key); });
        return r;
    }

    NOINLINE static
    void traverse (Reference& r, const Traversal& trav, const AnyString& key) {
        if (trav.desc->keys_offset) {
            return use_computed_attrs(r, trav, key);
        }
        else if (auto attrs = trav.desc->attrs()) {
            return use_attrs(r, trav, key, attrs);
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            return use_delegate(r, trav, key, acr);
        }
        else raise_AttrsNotSupported(trav.desc);
    }

    NOINLINE static
    void use_attrs (
        Reference& r, const Traversal& trav, const AnyString& key,
        const AttrsDcrPrivate* attrs
    ) {
         // First check direct attrs
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (attr->key == key) {
                trav_attr(trav, attr->acr(), attr->key, AccessMode::Read,
                    [&r](const Traversal& child)
                { r = child.to_reference(); });
                return;
            }
        }
         // Then included attrs
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (!!(acr->attr_flags & AttrFlags::Include)) {
                trav_attr(trav, acr, attr->key, AccessMode::Read,
                    [&r, &key](const Traversal& child)
                { traverse(r, child, key); });
                if (r) return;
            }
        }
        [[unlikely]] return;
    }

    NOINLINE static
    void use_computed_attrs (
        Reference& r, const Traversal& trav, const AnyString& key
    ) {
        expect(trav.desc->computed_attrs_offset);
        auto f = trav.desc->computed_attrs()->f;
        if (Reference ref = f(*trav.address, key)) {
            trav_computed_attr(trav, move(ref), f, key, AccessMode::Read,
                [&r](const Traversal& child)
            { r = child.to_reference(); });
        }
    }

    NOINLINE static
    void use_delegate (
        Reference& r, const Traversal& trav, const AnyString& key,
        const Accessor* acr
    ) {
        trav_delegate(trav, acr, AccessMode::Read,
            [&r, &key](const Traversal& child)
        { traverse(r, child, key); });
    }
};

Reference item_maybe_attr (
    const Reference& item, const AnyString& key, LocationRef loc
) {
    return TraverseAttr::start(item, key, loc);
}

Reference item_attr (const Reference& item, const AnyString& key, LocationRef loc) {
    Reference r = TraverseAttr::start(item, key, loc);
    if (!r) {
        try { raise_AttrNotFound(item.type(), key); }
        catch (...) { rethrow_with_travloc(loc); }
    }
    return r;
}

///// GET LENGTH

namespace in {

struct TraverseGetLength {
    static
    usize start (const Reference& item, LocationRef loc) try {
        if (auto addr = item.address()) {
            return traverse(*addr, item.type());
        }
        else {
            usize len;
            item.read([&len, type{item.type()}](Mu& v){
                len = traverse(v, type);
            });
            return len;
        }
    } catch (...) { rethrow_with_travloc(loc); }

    NOINLINE static
    usize traverse (Mu& item, Type type) {
        auto desc = DescriptionPrivate::get(type);
        if (auto acr = desc->length_acr()) {
            usize len;
            acr->read(item, [&len](Mu& v){
                len = reinterpret_cast<const usize&>(v);
            });
            return len;
        }
        else if (auto elems = desc->elems()) {
            return elems->chop_flag(AttrFlags::Invisible);
        }
        else if (auto acr = desc->delegate_acr()) {
            usize len;
            Type child_type = acr->type(&item);
            acr->read(item, [&len, child_type](Mu& v){
                len = traverse(v, child_type);
            });
            return len;
        }
        else raise_ElemsNotSupported(type);
    }
};

} // in

usize item_get_length (const Reference& item, LocationRef loc) {
    return TraverseGetLength::start(item, loc);
}

///// SET LENGTH

namespace in {

struct TraverseSetLength {
    static
    void start (const Reference& item, usize len, LocationRef loc) try {
        if (auto addr = item.address()) {
            traverse(*addr, item.type(), len);
        }
        else {
            item.read([type{item.type()}, len](Mu& v){
                traverse(v, type, len);
            });
        }
    } catch (...) { rethrow_with_travloc(loc); }

    NOINLINE static
    void traverse (Mu& item, Type type, usize len) {
        auto desc = DescriptionPrivate::get(type);
        if (auto acr = desc->length_acr()) {
            if (!(acr->flags & AcrFlags::Readonly)) {
                acr->write(item, [len](Mu& v){
                    reinterpret_cast<usize&>(v) = len;
                });
            }
            else {
                 // For readonly length, just check that the provided length matches
                usize expected;
                acr->read(item, [&expected](Mu& v){
                    expected = reinterpret_cast<const usize&>(v);
                });
                if (len != expected) {
                    raise_LengthRejected(type, expected, expected, len);
                }
            }
        }
        else if (auto elems = desc->elems()) {
            usize min = elems->chop_flag(AttrFlags::Optional);
            if (len < min || len > elems->n_elems) {
                raise_LengthRejected(type, min, elems->n_elems, len);
            }
        }
        else if (auto acr = desc->delegate_acr()) {
            Type child_type = acr->type(&item);
            acr->modify(item, [child_type, len](Mu& v){
                traverse(v, child_type, len);
            });
        }
        else raise_ElemsNotSupported(type);
    }
};

} // in

void item_set_length (const Reference& item, usize len, LocationRef loc) {
    TraverseSetLength::start(item, len, loc);
}

///// ELEM

namespace in {

 // TODO: We don't need to use traversals for this, we can process the acrs
 // directly.
struct TraverseElem {
    NOINLINE static
    Reference start (const Reference& item, usize index, LocationRef loc) {
        Reference r;
        trav_start(item, loc, false, AccessMode::Read,
            [&r, index](const Traversal& trav)
        { traverse(r, trav, index); });
        return r;
    }

    NOINLINE static
    void traverse (Reference& r, const Traversal& trav, usize index) {
        if (auto length = trav.desc->length_acr()) {
            if (!!(trav.desc->flags & DescFlags::ElemsContiguous)) {
                use_contiguous_elems(r, trav, index, length);
            }
            else {
                use_computed_elems(r, trav, index);
            }
        }
        else if (auto elems = trav.desc->elems()) {
            use_elems(r, trav, index, elems);
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(r, trav, index, acr);
        }
        else raise_ElemsNotSupported(trav.desc);
    }

    NOINLINE static
    void use_elems (
        Reference& r, const Traversal& trav, usize index,
        const ElemsDcrPrivate* elems
    ) {
        if (index > elems->n_elems) return;
        trav_elem(trav, elems->elem(index)->acr(), index, AccessMode::Read,
            [&r](const Traversal& child)
        { r = child.to_reference(); });
    }

    NOINLINE static
    void use_contiguous_elems (
        Reference& r, const Traversal& trav, usize index,
        const Accessor* length_acr
    ) {
         // We have to read the length to do bounds checking.
        usize len;
        length_acr->read(*trav.address,
            CallbackRef<void(Mu& v)>(len, [](usize& len, Mu& v){
                len = reinterpret_cast<const usize&>(v);
            })
        );
        if (index >= len) return;
        expect(trav.desc->contiguous_elems_offset);
        auto f = trav.desc->contiguous_elems()->f;
        AnyPtr ptr = f(*trav.address);
        auto child_desc = DescriptionPrivate::get(ptr.type);
        ptr.address = (Mu*)((char*)ptr.address + index * child_desc->cpp_size);
        trav_contiguous_elem(trav, ptr, f, index, AccessMode::Read,
            [&r](const Traversal& child)
        { r = child.to_reference(); });
    }

    NOINLINE static
    void use_computed_elems (
        Reference& r, const Traversal& trav, usize index
    ) {
        expect(trav.desc->computed_elems_offset);
        auto f = trav.desc->computed_elems()->f;
        Reference ref = f(*trav.address, index);
        if (!ref) return;
        trav_computed_elem(trav, ref, f, index, AccessMode::Read,
            [&r](const Traversal& child)
        { r = child.to_reference(); });
    }

    NOINLINE static
    void use_delegate (
        Reference& r, const Traversal& trav, usize index,
        const Accessor* acr
    ) {
        trav_delegate(trav, acr, AccessMode::Read,
            [&r, index](const Traversal& child)
        { traverse(r, child, index); });
    }
};

} // in

Reference item_maybe_elem (
    const Reference& item, usize index, LocationRef loc
) {
    return TraverseElem::start(item, index, loc);
}

Reference item_elem (const Reference& item, usize index, LocationRef loc) {
    Reference r = TraverseElem::start(item, index, loc);
    if (!r) {
        try { raise_ElemNotFound(item.type(), index); }
        catch (...) { rethrow_with_travloc(loc); }
    }
    return r;
}

///// ERRORS

void raise_AttrsNotSupported (Type item_type) {
    raise(e_AttrsNotSupported, cat(
        "Item of type ", item_type.name(),
        " does not support behaving like an ", "object."
    ));
}

void raise_AttrMissing (Type item_type, const AnyString& key) {
    raise(e_AttrMissing, cat(
        "Item of type ", item_type.name(), " missing required key ", key
    ));
}

void raise_AttrRejected (Type item_type, const AnyString& key) {
    raise(e_AttrRejected, cat(
        "Item of type ", item_type.name(), " given unwanted key ", key
    ));
}

void raise_ElemsNotSupported (Type item_type) {
    raise(e_ElemsNotSupported, cat(
        "Item of type ", item_type.name(),
        " does not support behaving like an ", "array."
    ));
}

void raise_LengthRejected (Type item_type, usize min, usize max, usize got) {
    UniqueString mess = min == max ? cat(
        "Item of type ", item_type.name(), " given wrong length ", got,
        " (expected ", min, ")"
    ) : cat(
        "Item of type ", item_type.name(), " given wrong length ", got,
        " (expected between ", min, " and ", max, ")"
    );
    raise(e_LengthRejected, move(mess));
}

void raise_AttrNotFound (Type item_type, const AnyString& key) {
    raise(e_AttrNotFound, cat(
        "Item of type ", item_type.name(), " has no attribute with key ", key
    ));
}

void raise_ElemNotFound (Type item_type, usize index) {
    raise(e_ElemNotFound, cat(
        "Item of type ", item_type.name(), " has no element at index ", index
    ));
}

} using namespace ayu;
