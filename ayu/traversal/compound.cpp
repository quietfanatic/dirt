#include "compound.private.h"

#include "from-tree.h"
#include "to-tree.h"

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

    void start (const Reference& item, LocationRef loc) {
        Traversal::start(item, loc, false, AccessMode::Read,
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
        if (auto attrs = trav.desc->attrs()) {
            use_attrs(trav, attrs);
        }
        else if (auto acr = trav.desc->keys_acr()) {
            use_computed_attrs(trav, acr);
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
             // TODO: discard invisible attrs?
            auto acr = attr->acr();
            if (acr->attr_flags & AttrFlags::Include) {
                trav.follow_attr(acr, attr->key, AccessMode::Read,
                    [this](const Traversal& child)
                { traverse(child); });
            }
            else collect(attr->key);
        }
    }

    NOINLINE void use_computed_attrs (
        const Traversal& trav, const Accessor* keys_acr
    ) {
        keys_acr->read(*trav.address, [this](Mu& v){
            auto& item_keys = reinterpret_cast<const AnyArray<AnyString>&>(v);
            for (auto& key : item_keys) {
                collect(AnyString(key));
            }
        });
    }

    NOINLINE void use_delegate (
        const Traversal& trav, const Accessor* acr
    ) {
        trav.follow_delegate(acr, AccessMode::Read,
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

 // TEMP
void in::trav_collect_keys (UniqueArray<AnyString>& keys, const Traversal& trav) {
    reinterpret_cast<TraverseGetKeys&>(keys).traverse(trav);
}

///// SET KEYS

namespace in {

struct TraverseSetKeys {
    UniqueArray<AnyString> keys;

    void start (const Reference& item, LocationRef loc) {
        Traversal::start(item, loc, false, AccessMode::Read,
            [this](const Traversal& trav)
        {
            traverse(trav, false);
            if (keys) raise_AttrRejected(trav.desc, keys[0]);
        });
    }

    bool claim (Str key) {
         // This algorithm overall is O(N^3), we may be able to speed it up by
         // setting a flag if there are no included attrs, or maybe by using an
         // unordered_set?
         // TODO: Just use a bool array for claiming instead of erasing from
         // the array?
        for (usize i = 0; i < keys.size(); ++i) {
            if (keys[i] == key) {
                keys.erase(i);
                return true;
            }
        }
        return false;
    }

    NOINLINE void traverse (const Traversal& trav, bool optional) {
        if (auto attrs = trav.desc->attrs()) {
            use_attrs(trav, optional, attrs);
        }
        else if (auto acr = trav.desc->keys_acr()) {
            use_computed_attrs(trav, optional, acr);
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(trav, optional, acr);
        }
        else raise_AttrsNotSupported(trav.desc);
    }

    NOINLINE void use_attrs (
        const Traversal& trav, bool optional, const AttrsDcrPrivate* attrs
    ) {
         // Prioritize direct attrs
         // I don't think it's possible for n_attrs to be large enough to
         // overflow the stack...right?  The max description size is 64K and an
         // attr always consumes at least 14 bytes, so the max n_attrs is
         // something like 4500.  TODO: enforce a reasonable max n_attrs in
         // descriptors-internal.h.
        bool claimed_included [attrs->n_attrs] = {};
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (claim(attr->key)) {
                 // If any attrs are given, all required attrs must be given
                 // (only matters if this item is an included attr)
                 // TODO: this should fail a test depending on the order of attrs
                optional = false;
                if (acr->attr_flags & AttrFlags::Include) {
                    claimed_included[i] = true;
                }
            }
            else if (optional || acr->attr_flags & (
                AttrFlags::Optional|AttrFlags::Include
            )) {
                 // Allow omitting optional or included attrs
            }
            else raise_AttrMissing(trav.desc, attr->key);
        }
         // Then check included attrs
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (acr->attr_flags & AttrFlags::Include) {
                 // Skip if attribute was given directly, uncollapsed
                if (claimed_included[i]) continue;
                bool opt = optional | !!(acr->attr_flags & AttrFlags::Optional);
                trav.follow_attr(acr, attr->key, AccessMode::Write,
                    [this, opt](const Traversal& child)
                { traverse(child, opt); });
            }
        }
    }

    NOINLINE void use_computed_attrs (
        const Traversal& trav, bool optional, const Accessor* keys_acr
    ) {
        if (!(keys_acr->flags & AcrFlags::Readonly)) {
            keys_acr->write(*trav.address, [this](Mu& v){
                reinterpret_cast<AnyArray<AnyString>&>(v) = move(keys);
            });
        }
        else {
             // For readonly keys, get the keys and compare them.
             // TODO: This can probably be optimized more
            TraverseGetKeys tgk;
            tgk.traverse(trav);
            for (auto& key : tgk.keys) {
                if (claim(key)) {
                     // If any of the keys are present, it makes this item no
                     // longer optional.
                    optional = false;
                }
                else if (!optional) raise_AttrMissing(trav.desc, key);
            }
            return;
        }
    }

    NOINLINE void use_delegate (
        const Traversal& trav, bool optional, const Accessor* acr
    ) {
        trav.follow_delegate(acr, AccessMode::Write,
            [this, optional](const Traversal& child)
        { traverse(child, optional); });
    }

};

} using namespace in;

void item_set_keys (
    const Reference& item, AnyArray<AnyString> keys, LocationRef loc
) {
    TraverseSetKeys(move(keys)).start(item, loc);
}

///// ATTR

Reference item_maybe_attr (
    const Reference& item, const AnyString& key, LocationRef loc
) {
    Reference r;
     // Is AccessMode::Read correct here?  Will we instead have to chain up the
     // reference from the start?
    Traversal::start(item, loc, false, AccessMode::Read,
        [&r, &key](const Traversal& trav)
    { trav_maybe_attr(trav, key, AccessMode::Read, ReceiveReference(r)); });
    return r;
}
Reference item_attr (const Reference& item, const AnyString& key, LocationRef loc) {
    Reference r;
    Traversal::start(item, loc, false, AccessMode::Read,
        [&r, &key](const Traversal& trav)
    { trav_attr(trav, key, AccessMode::Read, ReceiveReference(r)); });
    return r;
}

///// GET LENGTH

namespace in {

struct TraverseGetLength {
    static usize start (const Reference& item, LocationRef loc) try {
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

    NOINLINE static usize traverse (Mu& item, Type type) {
        auto desc = DescriptionPrivate::get(type);
        if (auto elems = desc->elems()) {
            return elems->n_elems;
        }
        else if (auto acr = desc->length_acr()) {
            usize len;
            acr->read(item, [&len](Mu& v){
                len = reinterpret_cast<const usize&>(v);
            });
            return len;
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
    static void start (const Reference& item, usize len, LocationRef loc) try {
        if (auto addr = item.address()) {
            traverse(*addr, item.type(), len);
        }
        else {
            item.read([type{item.type()}, len](Mu& v){
                traverse(v, type, len);
            });
        }
    } catch (...) { rethrow_with_travloc(loc); }

    NOINLINE static void traverse (Mu& item, Type type, usize len) {
        auto desc = DescriptionPrivate::get(type);
        if (auto elems = desc->elems()) {
            usize min = elems->n_elems;
             // Scan backwards for optional elements.  TODO: this could be done
             // at compile-time.
            while (min > 0 &&
                elems->elem(min-1)->acr()->attr_flags & AttrFlags::Optional
            ) min -= 1;
            if (len < min || len > elems->n_elems) {
                raise_LengthRejected(type, min, elems->n_elems, len);
            }
        }
        else if (auto acr = desc->length_acr()) {
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

Reference item_maybe_elem (
    const Reference& item, usize index, LocationRef loc
) {
    Reference r;
    Traversal::start(item, loc, false, AccessMode::Read,
        [&r, index](const Traversal& trav)
    { trav_maybe_elem(trav, index, AccessMode::Read, ReceiveReference(r)); });
    return r;
}
Reference item_elem (const Reference& item, usize index, LocationRef loc) {
    Reference r;
    Traversal::start(item, loc, false, AccessMode::Read,
        [&r, index](const Traversal& trav)
    { trav_elem(trav, index, AccessMode::Read, ReceiveReference(r)); });
    return r;
}

///// ERRORS

void in::raise_AttrsNotSupported (Type item_type) {
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

void in::raise_ElemsNotSupported (Type item_type) {
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

void in::raise_AttrNotFound (Type item_type, const AnyString& key) {
    raise(e_AttrNotFound, cat(
        "Item of type ", item_type.name(), " has no attribute with key ", key
    ));
}

void in::raise_ElemNotFound (Type item_type, usize index) {
    raise(e_ElemNotFound, cat(
        "Item of type ", item_type.name(), " has no element at index ", index
    ));
}

} using namespace ayu;
