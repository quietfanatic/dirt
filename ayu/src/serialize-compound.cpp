#include "serialize-compound-private.h"

#include "../serialize-from-tree.h"
#include "../serialize-to-tree.h"

namespace ayu {
using namespace in;

 // Pulling out this callback to avoid redundant instantiations of
 // ser_maybe_attr<> and ser_maybe_elem<> due to all lambdas having unique
 // types.
struct ReceiveReference {
    Reference& r;
    void operator() (const Traversal& child) {
        new (&r) Reference(child.to_reference());
    }
};

///// ATTRS

void in::ser_collect_key (UniqueArray<AnyString>& keys, AnyString&& key) {
     // This'll end up being N^2.  TODO: Test whether including an unordered_set
     // would speed this up (probably not).  Maybe even just hashing the key
     // might be enough.
    for (auto k : keys) if (k == key) return;
    keys.emplace_back(move(key));
}

void in::ser_collect_keys (
    const Traversal& trav, UniqueArray<AnyString>& keys
) {
    if (auto acr = trav.desc->keys_acr()) {
        Type keys_type = acr->type(trav.address);
         // Compare Type not std::type_info, since std::type_info can require a
         // string comparison.
        if (keys_type == Type::CppType<AnyArray<AnyString>>()) {
             // Optimize for AnyArray<AnyString>
            acr->read(*trav.address, [&keys](Mu& v){
                auto& item_keys = reinterpret_cast<const AnyArray<AnyString>&>(v);
                for (auto& key : item_keys) {
                    ser_collect_key(keys, AnyString(key));
                }
            });
        }
        else [[unlikely]] {
             // General case, any type that serializes to an array of strings.
            acr->read(*trav.address, [keys_type, &keys](Mu& v){
                 // We might be able to optimize this more, but it's not that
                 // important.
                auto keys_tree = item_to_tree(Pointer(keys_type, &v));
                if (keys_tree.form != ARRAY) throw InvalidKeysType(keys_type);
                for (const Tree& key : TreeArraySlice(keys_tree)) {
                    if (key.form != STRING) throw InvalidKeysType(keys_type);
                    ser_collect_key(keys, AnyString(move(key)));
                }
            });
        }
    }
    else if (auto attrs = trav.desc->attrs()) {
        for (uint16 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (acr->attr_flags & ATTR_INCLUDE) {
                trav.follow_attr(acr, attr->key, ACR_READ,
                    [&keys](const Traversal& child)
                { ser_collect_keys(child, keys); });
            }
            else ser_collect_key(keys, attr->key);
        }
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        trav.follow_delegate(acr, ACR_READ, [&keys](const Traversal& child){
            ser_collect_keys(child, keys);
        });
    }
    else throw NoAttrs();
}

AnyArray<AnyString> item_get_keys (
    const Reference& item, LocationRef loc
) {
    UniqueArray<AnyString> keys;
    Traversal::start(item, loc, false, ACR_READ,
        [&keys](const Traversal& trav)
    { ser_collect_keys(trav, keys); });
    return keys;
}

bool in::ser_claim_key (UniqueArray<AnyString>& keys, Str key) {
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

void in::ser_claim_keys (
    const Traversal& trav,
    UniqueArray<AnyString>& keys,
    bool optional
) {
    if (auto acr = trav.desc->keys_acr()) {
        Type keys_type = acr->type(trav.address);
        if (!(acr->accessor_flags & ACR_READONLY)) {
            if (keys_type == Type::CppType<AnyArray<AnyString>>()) {
                 // Optimize for AnyArray<AnyString>
                acr->write(*trav.address, [&keys](Mu& v){
                    reinterpret_cast<AnyArray<AnyString>&>(v) = keys;
                });
            }
            else [[unlikely]] {
                 // General case: call item_from_tree on the keys.  This will
                 // be slow.
                UniqueArray<Tree> array (keys.size());
                for (usize i = 0; i < keys.size(); i++) {
                    array[i] = Tree(keys[i]);
                }
                acr->write(*trav.address, [keys_type, &array](Mu& v){
                    item_from_tree(
                        Pointer(keys_type, &v), Tree(move(array))
                    );
                });
            }
            keys = {};
        }
        else {
             // For readonly keys, get the keys and compare them.
             // TODO: This can probably be optimized more
            UniqueArray<AnyString> required_keys;
            ser_collect_keys(trav, required_keys);
            for (auto& key : required_keys) {
                if (ser_claim_key(keys, key)) {
                     // If any of the keys are present, it makes this item no
                     // longer optional.
                    optional = false;
                }
                else if (!optional) throw MissingAttr(key);
            }
            return;
        }
    }
    else if (auto attrs = trav.desc->attrs()) {
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
            if (ser_claim_key(keys, attr->key)) {
                 // If any attrs are given, all required attrs must be given
                 // (only matters if this item is an included attr)
                 // TODO: this should fail a test depending on the order of attrs
                optional = false;
                if (acr->attr_flags & ATTR_INCLUDE) {
                    claimed_included[i] = true;
                }
            }
            else if (optional || acr->attr_flags & (ATTR_OPTIONAL|ATTR_INCLUDE)) {
                 // Allow omitting optional or included attrs
            }
            else throw MissingAttr(attr->key);
        }
         // Then check included attrs
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (acr->attr_flags & ATTR_INCLUDE) {
                 // Skip if attribute was given directly, uncollapsed
                if (claimed_included[i]) continue;
                bool opt = optional | !!(acr->attr_flags & ATTR_OPTIONAL);
                trav.follow_attr(acr, attr->key, ACR_WRITE,
                    [&keys, opt](const Traversal& child)
                { ser_claim_keys(child, keys, opt); });
            }
        }
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        trav.follow_delegate(acr, ACR_WRITE,
            [&keys, optional](const Traversal& child)
        { ser_claim_keys(child, keys, optional); });
    }
    else throw NoAttrs();
}

void in::ser_set_keys (
    const Traversal& trav, UniqueArray<AnyString>&& keys
) {
    ser_claim_keys(trav, keys, false);
    if (keys) throw_noinline<UnwantedAttr>(keys[0]);
}

void item_set_keys (
    const Reference& item, AnyArray<AnyString> keys, LocationRef loc
) {
    Traversal::start(item, loc, false, ACR_WRITE,
        [&keys](const Traversal& trav)
    { ser_set_keys(trav, move(keys)); });
}

Reference item_maybe_attr (
    const Reference& item, AnyString key, LocationRef loc
) {
    Reference r;
     // Is ACR_READ correct here?  Will we instead have to chain up the
     // reference from the start?
    Traversal::start(item, loc, false, ACR_READ,
        [&r, &key](const Traversal& trav)
    { ser_maybe_attr(trav, key, ACR_READ, ReceiveReference(r)); });
    return r;
}
Reference item_attr (const Reference& item, AnyString key, LocationRef loc) {
    Reference r;
    Traversal::start(item, loc, false, ACR_READ,
        [&r, &key](const Traversal& trav)
    { ser_attr(trav, key, ACR_READ, ReceiveReference(r)); });
    return r;
}

///// ELEMS

usize in::ser_get_length (const Traversal& trav) {
    if (auto acr = trav.desc->length_acr()) {
        usize len;
         // Do we want to support other integral types besides usize?  Probably
         // not very high priority.
        acr->read(*trav.address, [&len](Mu& v){
            len = reinterpret_cast<const usize&>(v);
        });
        return len;
    }
    else if (auto elems = trav.desc->elems()) {
        return elems->n_elems;
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        usize len;
        trav.follow_delegate(acr, ACR_READ, [&len](const Traversal& child){
            len = ser_get_length(child);
        });
        return len;
    }
    else throw NoElems();
}

usize item_get_length (const Reference& item, LocationRef loc) {
    usize len;
    Traversal::start(item, loc, false, ACR_READ, [&len](const Traversal& trav){
        len = ser_get_length(trav);
    });
    return len;
}

void in::ser_set_length (const Traversal& trav, usize len) {
    if (auto acr = trav.desc->length_acr()) {
        if (!(acr->accessor_flags & ACR_READONLY)) {
            acr->write(*trav.address, [len](Mu& v){
                reinterpret_cast<usize&>(v) = len;
            });
        }
        else {
             // For readonly length, just check that the provided length matches
            usize expected;
            acr->read(*trav.address, [&expected](Mu& v){
                expected = reinterpret_cast<const usize&>(v);
            });
            if (len != expected) {
                throw WrongLength(expected, expected, len);
            }
        }
    }
    else if (auto elems = trav.desc->elems()) {
        usize min = elems->n_elems;
         // Scan backwards for optional elements
        while (min > 0 &&
            elems->elem(min-1)->acr()->attr_flags & ATTR_OPTIONAL
        ) min -= 1;
        if (len < min || len > elems->n_elems) {
            throw WrongLength(min, elems->n_elems, len);
        }
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        trav.follow_delegate(acr, ACR_WRITE, [len](const Traversal& child){
            ser_set_length(child, len);
        });
    }
    else throw NoElems();
}

void item_set_length (const Reference& item, usize len, LocationRef loc) {
    Traversal::start(item, loc, false, ACR_WRITE, [len](const Traversal& trav){
        ser_set_length(trav, len);
    });
}

Reference item_maybe_elem (
    const Reference& item, usize index, LocationRef loc
) {
    Reference r;
    Traversal::start(item, loc, false, ACR_READ,
        [&r, index](const Traversal& trav)
    { ser_maybe_elem(trav, index, ACR_READ, ReceiveReference(r)); });
    return r;
}
Reference item_elem (const Reference& item, usize index, LocationRef loc) {
    Reference r;
    Traversal::start(item, loc, false, ACR_READ,
        [&r, index](const Traversal& trav)
    { ser_elem(trav, index, ACR_READ, ReceiveReference(r)); });
    return r;
}

} using namespace ayu;
