#include "compound.private.h"
#include "../reflection/describe-standard.h"
#include "../reflection/description.private.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

///// GET KEYS

struct GetKeysTraversalHead {
    UniqueArray<AnyString>* keys;
};

template <class T = Traversal>
struct GetKeysTraversal : GetKeysTraversalHead, T { };

struct TraverseGetKeys {

    static
    UniqueArray<AnyString> start (const AnyRef& item, RouteRef rt) {
        CurrentBase curb (rt, item);
         // TODO: skip traversal if item is addressable and uses computed_attrs
        UniqueArray<AnyString> keys;
        GetKeysTraversal<StartTraversal> child;
        child.keys = &keys;
        trav_start<visit>(child, item, rt, AC::Read);
        return keys;
    }

    static
    void collect (UniqueArray<AnyString>& keys, AnyString&& key) {
         // This'll end up being N^2.  TODO: Test whether including an unordered_set
         // would speed this up (probably not).  Maybe even just hashing the key
         // might be enough.
         //
         // TODO: There generally aren't supposed to be any duplicates, can we
         // optimize for the case where there aren't?
        for (auto k : keys) if (k == key) return;
        keys.emplace_back(move(key));
    }

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const GetKeysTraversal<>&>(tr);
        auto desc = trav.desc();
        if (auto acr = desc->keys_acr()) {
            use_computed_attrs(trav, acr);
        }
        else if (auto attrs = desc->attrs()) {
            use_attrs(trav, attrs);
        }
        else if (auto acr = desc->delegate_acr()) {
            use_delegate(trav, acr);
        }
        else raise_AttrsNotSupported(trav.type);
    }

    NOINLINE static
    void use_attrs (
        const GetKeysTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
        for (u16 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (acr->attr_flags % AttrFlags::Invisible) continue;
            if (acr->attr_flags % AttrFlags::Include) {
                GetKeysTraversal<AttrTraversal> child;
                child.keys = trav.keys;
                trav_attr<visit>(child, trav, acr, attr->key, AC::Read);
            }
            else collect(*trav.keys, attr->key);
        }
    }

    static // not noinline because it's just one call
    void use_computed_attrs (
        const GetKeysTraversal<>& trav, const Accessor* keys_acr
    ) {
        keys_acr->read(*trav.address,
            AccessCB(*trav.keys, [](auto& keys, Type t, Mu* v)
        {
            auto& ks = require_readable_keys(t, v);
            for (auto& key : ks) collect(keys, AnyString(key));
        }));
    }

    NOINLINE static
    void use_delegate (
        const GetKeysTraversal<>& trav, const Accessor* acr
    ) {
        GetKeysTraversal<DelegateTraversal> child;
        child.keys = trav.keys;
        trav_delegate<visit>(child, trav, acr, AC::Read);
    }
};

} using namespace in;

NOINLINE
AnyArray<AnyString> item_get_keys (
    const AnyRef& item, RouteRef rt
) {
    return TraverseGetKeys::start(item, rt);
}

///// SET KEYS

namespace in {

struct SetKeysTraversalHead {
     // Not const because this is a consuming algorithm
    UniqueArray<AnyString>* keys;
};
template <class T = Traversal>
struct SetKeysTraversal : SetKeysTraversalHead, T { };

struct TraverseSetKeys {

    static
    void start (
        const AnyRef& item, AnyArray<AnyString> ks, RouteRef rt
    ) {
        CurrentBase curb (rt, item);
        UniqueArray<AnyString> keys = move(ks);
        SetKeysTraversal<StartTraversal> child;
        child.keys = &keys;
        trav_start<visit_and_verify>(child, item, rt, AC::Read);
    }

    static
    bool claim (UniqueArray<AnyString>& keys, Str key) {
         // This algorithm overall is O(N^3), we may be able to speed it up by
         // setting a flag if there are no included attrs, or maybe by using an
         // unordered_set?
         // TODO: Use a next_list like in from-tree.
        for (u32 i = 0; i < keys.size(); ++i) {
            if (keys[i] == key) {
                keys.erase(i);
                return true;
            }
        }
        return false;
    }

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const SetKeysTraversal<>&>(tr);
        auto desc = trav.desc();
        if (auto acr = desc->keys_acr()) {
            if (acr->caps % AC::Write) {
                use_computed_attrs(trav, acr);
            }
            else {
                use_computed_attrs_readonly(trav, acr);
            }
        }
        else if (auto attrs = desc->attrs()) {
            use_attrs(trav, attrs);
        }
        else if (auto acr = desc->delegate_acr()) {
            use_delegate(trav, acr);
        }
        else raise_AttrsNotSupported(trav.type);
    }

    NOINLINE static
    void visit_and_verify (const Traversal& tr) {
        visit(tr);
        auto& trav = static_cast<const SetKeysTraversal<>&>(tr);
        if (*trav.keys) raise_AttrRejected(trav.type, (*trav.keys)[0]);
    }

    NOINLINE static
    void use_attrs (
        const SetKeysTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
         // Prioritize direct attrs
         // I don't think it's possible for n_attrs to be large enough to
         // overflow the stack...right?  The max description size is 64K and an
         // attr always consumes at least 14 bytes, so the max n_attrs is
         // something like 4500.  TODO: enforce a reasonable max n_attrs in
         // descriptors-internal.h.
        bool claimed [attrs->n_attrs] = {};
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (claim(*trav.keys, attr->key)) {
                claimed[i] = true;
            }
            else if (acr->attr_flags % (AttrFlags::Optional|AttrFlags::Include)) {
                 // Allow omitting optional or included attrs
            }
            else raise_AttrMissing(trav.type, attr->key);
        }
         // Then check included attrs
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (acr->attr_flags % AttrFlags::Include) {
                 // Skip if attribute was given directly, uncollapsed
                if (claimed[i]) continue;
                GetKeysTraversal<AttrTraversal> child;
                child.keys = trav.keys;
                trav_attr<visit>(
                    child, trav, acr, attr->key, AC::Write
                );
            }
        }
    }

    static
    void use_computed_attrs (
        const SetKeysTraversal<>& trav, const Accessor* keys_acr
    ) {
        keys_acr->write(*trav.address,
            AccessCB(move(*trav.keys), [](auto&& keys, Type t, Mu* v)
        {
            auto& ks = require_writeable_keys(t, v);
            ks = move(keys);
        }));
    }

    NOINLINE static
    void use_computed_attrs_readonly (
        const SetKeysTraversal<>& trav, const Accessor* keys_acr
    ) {
         // For readonly keys, get the keys and compare them.  This code is
         // copied from set_keys_readonly in from-tree.cpp.  I don't care enough
         // about this codepath to work any harder on it.
        AnyArray<AnyString> keys;
        keys_acr->read(*trav.address,
            AccessCB(keys, [](auto& keys, Type t, Mu* v)
        {
            auto& ks = require_writeable_keys(t, v);
            new (&keys) AnyArray<AnyString>(ks);
        }));
#ifndef NDEBUG
         // Check returned keys for duplicates
        for (u32 i = 0; i < keys.size(); i++)
        for (u32 j = 0; j < i; j++) {
            expect(keys[i] != keys[j]);
        }
#endif
        if (keys.size() >= trav.keys->size()) {
            for (auto& required : keys) {
                for (auto& given : *trav.keys) {
                    if (given == required) goto next_required;
                }
                raise_AttrMissing(trav.type, required);
                next_required:;
            }
        }
        else [[unlikely]] {
             // Too many keys given
            for (auto& given : *trav.keys) {
                for (auto& required : keys) {
                    if (required == given) goto next_given;
                }
                raise_AttrRejected(trav.type, given);
                next_given:;
            }
            never();
        }
    }

    NOINLINE static
    void use_delegate (
        const SetKeysTraversal<>& trav, const Accessor* acr
    ) {
        SetKeysTraversal<DelegateTraversal> child;
        child.keys = trav.keys;
        trav_delegate<visit>(child, trav, acr, AC::Write);
    }

};

} using namespace in;

void item_set_keys (
    const AnyRef& item, AnyArray<AnyString> keys, RouteRef rt
) {
    TraverseSetKeys::start(item, move(keys), rt);
}

///// ATTR

struct GetAttrTraversalHead {
    const AnyString* get_key;
};

template <class T = Traversal>
struct GetAttrTraversal : GetAttrTraversalHead, ReturnRefTraversal<T> { };

struct TraverseAttr {
    NOINLINE static
    AnyRef start (
        const AnyRef& item, const AnyString& key, RouteRef rt
    ) {
        CurrentBase curb (rt, item);
         // TODO: skip the traversal system if we're using computed attrs
        AnyRef r;
        GetAttrTraversal<StartTraversal> child;
        child.get_key = &key;
        child.r = &r;
        trav_start<visit>(child, item, rt, AC::Read);
        return r;
    }

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const GetAttrTraversal<>&>(tr);
        auto desc = trav.desc();
        if (desc->keys_acr()) {
            return use_computed_attrs(trav, expect(desc->computed_attrs())->f);
        }
        else if (auto attrs = desc->attrs()) {
            return use_attrs(trav, attrs);
        }
        else if (auto acr = desc->delegate_acr()) {
            return use_delegate(trav, acr);
        }
        else raise_AttrsNotSupported(trav.type);
    }

    NOINLINE static
    void use_attrs (
        const GetAttrTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
         // First check direct attrs
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (attr->key == *trav.get_key) {
                ReturnRefTraversal<AttrTraversal> child;
                child.r = trav.r;
                trav_attr<return_ref>(
                    child, trav, attr->acr(), attr->key, AC::Read
                );
                return;
            }
        }
         // Then included attrs
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (acr->attr_flags % AttrFlags::Include) {
                GetAttrTraversal<AttrTraversal> child;
                child.get_key = trav.get_key;
                child.r = trav.r;
                trav_attr<visit>(child, trav, acr, attr->key, AC::Read);
                if (*child.r) return;
            }
        }
        [[unlikely]] return;
    }

    NOINLINE static
    void use_computed_attrs (const GetAttrTraversal<>& trav, AttrFunc<Mu>* f) {
        if (AnyRef ref = f(*trav.address, *trav.get_key)) {
            ReturnRefTraversal<ComputedAttrTraversal> child;
            child.r = trav.r;
            trav_computed_attr<return_ref>(
                child, trav, move(ref), f, *trav.get_key, AC::Read
            );
        }
    }

    NOINLINE static
    void use_delegate (
        const GetAttrTraversal<>& trav, const Accessor* acr
    ) {
        GetAttrTraversal<DelegateTraversal> child;
        child.get_key = trav.get_key;
        child.r = trav.r;
        trav_delegate<visit>(child, trav, acr, AC::Read);
    }
};

NOINLINE
AnyRef item_maybe_attr (
    const AnyRef& item, const AnyString& key, RouteRef rt
) {
    return TraverseAttr::start(item, key, rt);
}

NOINLINE
AnyRef item_attr (const AnyRef& item, const AnyString& key, RouteRef rt) {
    AnyRef r = TraverseAttr::start(item, key, rt);
    if (!r) {
        try { raise_AttrNotFound(item.type(), key); }
        catch (...) { rethrow_with_route(rt); }
    }
    return r;
}

///// GET LENGTH

namespace in {

 // This is simple enough we don't need to use the traversal system.
struct TraverseGetLength {
    static
    u32 start (const AnyRef& item, RouteRef rt) try {
         // We still need to set current base in case user code is called,
         // because it's API-visible.
        CurrentBase curb (rt, item);
        u32 len;
        item.read(AccessCB(len, &visit));
        return len;
    } catch (...) { rethrow_with_route(rt); }

    static
    void visit (u32& len, Type t, Mu* v) {
        auto desc = DescriptionPrivate::get(t);
        if (auto acr = desc->length_acr()) {
            read_length_acr(len, t, v, acr);
        }
        else if (auto elems = desc->elems()) {
            len = elems->chop_flag(AttrFlags::Invisible);
        }
        else if (auto acr = desc->delegate_acr()) {
            acr->read(*v, AccessCB(len, &visit));
        }
        else raise_ElemsNotSupported(t);
    }
};

} // in

u32 item_get_length (const AnyRef& item, RouteRef rt) {
    return TraverseGetLength::start(item, rt);
}

///// SET LENGTH

namespace in {

struct TraverseSetLength {
    static
    void start (const AnyRef& item, u32 len, RouteRef rt) try {
        CurrentBase curb (rt, item);
        item.read(AccessCB(len, &visit));
    } catch (...) { rethrow_with_route(rt); }

    NOINLINE static
    void visit (u32& len, Type t, Mu* v) {
        auto desc = DescriptionPrivate::get(t);
        if (auto acr = desc->length_acr()) {
            write_length_acr(len, t, v, acr);
        }
        else if (auto elems = desc->elems()) {
            u32 min = elems->chop_flag(AttrFlags::Optional);
            if (len < min || len > elems->n_elems) {
                raise_LengthRejected(t, min, elems->n_elems, len);
            }
        }
        else if (auto acr = desc->delegate_acr()) {
             // TODO: enforce nonreadonly
            acr->modify(*v, AccessCB(len, &visit));
        }
        else raise_ElemsNotSupported(t);
    }
};

} // in

void item_set_length (const AnyRef& item, u32 len, RouteRef rt) {
    if (len > AnyArray<Tree>::max_size_) {
        raise_LengthOverflow(len);
    }
    TraverseSetLength::start(item, len, rt);
}

///// ELEM

namespace in {

struct GetElemTraversalHead {
    u32 index;
};

template <class T = Traversal>
struct GetElemTraversal : GetElemTraversalHead, ReturnRefTraversal<T> { };

 // TODO: Skip the traversal system for some cases
struct TraverseElem {

    NOINLINE static
    void start (const AnyRef& item, u32 index, RouteRef rt, AnyRef& r) {
        CurrentBase curb (rt, item);
        GetElemTraversal<StartTraversal> child;
        child.index = index;
        child.r = &r;
        trav_start<visit>(child, item, rt, AC::Read);
    }

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const GetElemTraversal<>&>(tr);
        auto desc = trav.desc();
        if (auto length = desc->length_acr()) {
            if (desc->flags % DescFlags::ElemsContiguous) {
                use_contiguous_elems(trav, length);
            }
            else {
                use_computed_elems(trav, expect(desc->computed_elems())->f);
            }
        }
        else if (auto elems = desc->elems()) {
            use_elems(trav, elems);
        }
        else if (auto acr = desc->delegate_acr()) {
            use_delegate(trav, acr);
        }
        else raise_ElemsNotSupported(trav.type);
    }

    NOINLINE static
    void use_elems (
        const GetElemTraversal<>& trav, const ElemsDcrPrivate* elems
    ) {
        if (trav.index > elems->n_elems) return;
        auto acr = elems->elem(trav.index)->acr();
        ReturnRefTraversal<ElemTraversal> child;
        child.r = trav.r;
        trav_elem<return_ref>(child, trav, acr, trav.index, AC::Read);
    }

    NOINLINE static
    void use_computed_elems (const GetElemTraversal<>& trav, ElemFunc<Mu>* f) {
        AnyRef ref = f(*trav.address, trav.index);
        if (!ref) return;
        ReturnRefTraversal<ComputedElemTraversal> child;
        child.r = trav.r;
        trav_computed_elem<return_ref>(
            child, trav, ref, f, trav.index, AC::Read
        );
    }

    NOINLINE static
    void use_contiguous_elems (
        const GetElemTraversal<>& trav, const Accessor* length_acr
    ) {
         // We have to read the length to do bounds checking, making this
         // ironically slower than computed_elems.
        u32 len;
        read_length_acr(len, trav.type, trav.address, length_acr);
        if (trav.index >= len) return;
        auto f = expect(trav.desc()->contiguous_elems())->f;
        AnyPtr ptr = f(*trav.address);
        ptr.address = (Mu*)(
            (char*)ptr.address + trav.index * ptr.type().cpp_size()
        );
        ReturnRefTraversal<ContiguousElemTraversal> child;
        child.r = trav.r;
        trav_contiguous_elem<return_ref>(
            child, trav, ptr, f, trav.index, AC::Read
        );
    }

    NOINLINE static
    void use_delegate (
        const GetElemTraversal<>& trav, const Accessor* acr
    ) {
        GetElemTraversal<DelegateTraversal> child;
        child.index = trav.index;
        child.r = trav.r;
        trav_delegate<visit>(child, trav, acr, AC::Read);
    }
};

} // in

AnyRef item_maybe_elem (
    const AnyRef& item, u32 index, RouteRef rt
) {
    AnyRef r;
    TraverseElem::start(item, index, rt, r);
    return r;
}

AnyRef item_elem (const AnyRef& item, u32 index, RouteRef rt) {
    AnyRef r;
    TraverseElem::start(item, index, rt, r);
    if (!r) {
        try { raise_ElemNotFound(item.type(), index); }
        catch (...) { rethrow_with_route(rt); }
    }
    return r;
}

///// LENGTH AND KEYS ACR HANDLING

void in::read_length_acr_cb (u32& len, Type t, Mu* v) {
    u64 l;
    if (t == Type::For<u32>()) {
        l = reinterpret_cast<const u32&>(*v);
    }
    else if (t == Type::For<u64>()) {
        l = reinterpret_cast<const u64&>(*v);
    }
    else raise_LengthTypeInvalid(Type(), t);
    if (l > AnyArray<Tree>::max_size_) {
        raise_LengthOverflow(l);
    }
    len = l;
}

void in::write_length_acr_cb (u32& len, Type t, Mu* v) {
    expect(len <= AnyArray<Tree>::max_size_);
    if (t == Type::For<u32>()) {
        reinterpret_cast<u32&>(*v) = len;
    }
    else if (t == Type::For<u64>()) {
        reinterpret_cast<u64&>(*v) = len;
    }
    else {
        raise_LengthTypeInvalid(Type(), t);
    }
}

///// ERRORS

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

void raise_LengthRejected (Type item_type, u32 min, u32 max, u32 got) {
    UniqueString mess = min == max ? cat(
        "Item of type ", item_type.name(), " given wrong length ", got,
        " (expected ", min, ")"
    ) : cat(
        "Item of type ", item_type.name(), " given wrong length ", got,
        " (expected between ", min, " and ", max, ")"
    );
    raise(e_LengthRejected, move(mess));
}

void raise_KeysTypeInvalid (Type, Type got_type) {
    raise(e_KeysTypeInvalid, cat(
        "Item has keys accessor of wrong type; expected AnyArray<AnyString> but got ",
        got_type.name()
    ));
}

void raise_AttrNotFound (Type item_type, const AnyString& key) {
    raise(e_AttrNotFound, cat(
        "Item of type ", item_type.name(), " has no attribute with key ", key
    ));
}

void raise_AttrsNotSupported (Type item_type) {
    raise(e_AttrsNotSupported, cat(
        "Item of type ", item_type.name(),
        " does not support behaving like an ", "object"
    ));
}

void raise_ElemNotFound (Type item_type, u32 index) {
    raise(e_ElemNotFound, cat(
        "Item of type ", item_type.name(), " has no element at index ", index
    ));
}

void raise_ElemsNotSupported (Type item_type) {
    raise(e_ElemsNotSupported, cat(
        "Item of type ", item_type.name(),
        " does not support behaving like an ", "array"
    ));
}

void raise_LengthTypeInvalid (Type, Type got_type) {
    raise(e_LengthTypeInvalid, cat(
        "Item has length accessor of wrong type; expected u32 or u64 but got ",
        got_type.name()
    ));
}

void raise_LengthOverflow (u64 len) {
    raise(e_LengthOverflow, cat(
        "Item's length is far too large (", len, " > 0x7fffffff)"
    ));
}

} using namespace ayu;

 // Force instantiation of the keys type
AYU_DESCRIBE_INSTANTIATE(AnyArray<AnyString>)

