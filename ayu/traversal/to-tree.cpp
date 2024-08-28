#include "to-tree.h"

#include "../reflection/descriptors.private.h"
#include "compound.h"
#include "scan.h"
#include "traversal2.private.h"

namespace ayu {
namespace in {

static uint64 diagnostic_serialization = 0;

struct ToTreeTraversal : FollowingTraversal<ToTreeTraversal> {
    Tree* r;

     // NOINLINE this because it generates a lot of code with the follow_start
    NOINLINE static
    Tree start (const AnyRef& item, LocationRef loc) {
        plog("to_tree start");
        PushBaseLocation pbl(loc ? loc : LocationRef(SharedLocation(item)));
        KeepLocationCache klc;
        Tree r;
        ToTreeTraversal child;
        child.r = &r;
        child.follow_start<&ToTreeTraversal::visit>(
            item, loc, false, AccessMode::Read
        );
        plog("to_tree end");
        return r;
    }

///// PICK STRATEGY

    NOINLINE
    void visit () try {
         // The majority of items are [[likely]] to be atomic.
        if (auto to_tree = desc->to_tree()) [[likely]] {
            use_to_tree(to_tree->f);
        }
        else if (auto values = desc->values()) {
            use_values(values);
        }
        else no_value_match();
    }
     // Unfortunately this exception handler prevents tail calling from this
     // function, but putting it anywhere else seems to perform worse.
    catch (...) { if (!wrap_exception()) throw; }

    NOINLINE
    void no_value_match () {
        if (desc->preference() == DescFlags::PreferObject) {
            if (auto keys = desc->keys_acr()) {
                return use_computed_attrs(keys);
            }
            else if (auto attrs = desc->attrs()) {
                return use_attrs(attrs);
            }
            else never();
        }
        else if (desc->preference() == DescFlags::PreferArray) {
            if (auto length = desc->length_acr()) {
                if (!!(desc->flags & DescFlags::ElemsContiguous)) {
                    return use_contiguous_elems(length);
                }
                else {
                    return use_computed_elems(length);
                }
            }
            else if (auto elems = desc->elems()) {
                return use_elems(elems);
            }
            else never();
        }
        else if (auto acr = desc->delegate_acr()) {
            use_delegate(acr);
        }
        else fail();
    }

///// STRATEGIES

    void use_to_tree (ToTreeFunc<Mu>* f) {
        new (r) Tree(f(*address));
    }

    NOINLINE
    void use_values (const ValuesDcrPrivate* values) {
        for (uint i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (values->compare(*address, *value->get_value())) {
                new (r) Tree(value->name);
                return;
            }
        }
        no_value_match();
    }

    NOINLINE
    void use_attrs (const AttrsDcrPrivate* attrs) {
        auto object = UniqueArray<TreePair>(Capacity(attrs->n_attrs));
         // First just build the object as though none of the attrs are included
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (!!(attr->acr()->attr_flags & AttrFlags::Invisible)) continue;

            ToTreeTraversal child;

            child.r = &object.emplace_back_expect_capacity(
                attr->key, Tree()
            ).second;
            child.follow_attr<&ToTreeTraversal::visit>(
                *this, attr->acr(), attr->key, AccessMode::Read
            );
            child.r->flags |= child.acr->tree_flags();
        }
         // Then if there are included or collapsed attrs, rebuild the object
         // while flattening them.
        if (!!(desc->flags & DescFlags::AttrsNeedRebuild)) {
             // Determine length for preallocation
            usize len = object.size();
            for (uint i = 0; i < attrs->n_attrs; i++) {
                auto flags = attrs->attr(i)->acr()->attr_flags;
                 // Ignore HasDefault; it can only decrease the length by 1, and
                 // checking whether it does requires comparing Trees, so I'd
                 // rather just overallocate.
                if (!!(flags &
                    (AttrFlags::Include|AttrFlags::CollapseOptional)
                )) {
                     // This works for both include and collapse_optional
                    len = len + (object[i].second.meta >> 1) - 1;
                }
            }
             // Allocate
            auto new_object = decltype(object)(Capacity(len));
             // Selectively flatten
            for (uint i = 0; i < attrs->n_attrs; i++) {
                auto attr = attrs->attr(i);
                auto flags = attr->acr()->attr_flags;
                auto key = move(object[i].first);
                Tree value = move(object[i].second);
                if (!!(flags & AttrFlags::Include)) {
                    if (value.form != Form::Object) {
                        raise(e_General,
                            "Included item did not serialize to an object"
                        );
                    }
                     // DON'T consume sub object because it could be shared.
                    for (auto& pair : AnyArray<TreePair>(move(value))) {
                        new_object.emplace_back_expect_capacity(pair);
                    }
                }
                else if (!!(flags & AttrFlags::CollapseOptional)) {
                    if (value.form != Form::Array || (value.meta >> 1) > 1) {
                        raise(e_General,
                            "Attribute with collapse_optional did not "
                            "serialize to an array of 0 or 1 elements"
                        );
                    }
                    if (auto a = AnyArray<Tree>(move(value))) {
                        new_object.emplace_back_expect_capacity(
                            move(key), a[0]
                        );
                    }
                    else { } // Drop the attr
                }
                else if (const Tree* def = attr->default_value()) {
                    if (value != *def) {
                        new_object.emplace_back_expect_capacity(
                            move(key), move(value)
                        );
                    }
                    else { } // Drop the attr
                }
                else {
                    new_object.emplace_back_expect_capacity(
                        move(key), move(value)
                    );
                }
            }
             // Old object's contents should be fully consumed so skip the
             // destructor loop (but verify in debug mode).
#ifndef NDEBUG
            for (auto& pair : object) {
                expect(!pair.first.owned());
                expect(!(pair.second.meta & 1));
            }
#endif
            SharableBuffer<TreePair>::deallocate(object.impl.data);
            new (&object) UniqueArray<TreePair>(move(new_object));
        }
         // This will check for duplicates in debug mode.
        new (r) Tree(move(object));
    }

    NOINLINE
    void use_computed_attrs (const Accessor* keys_acr) {
         // Get list of keys
        AnyArray<AnyString> keys;
        keys_acr->read(*address, CallbackRef<void(Mu&)>(
            keys, [](AnyArray<AnyString>& keys, Mu& v)
        {
            new (&keys) AnyArray<AnyString>(
                reinterpret_cast<const AnyArray<AnyString>&>(v)
            );
        }));
         // Now read value for each key
        auto object = UniqueArray<TreePair>(Capacity(keys.size()));
        expect(desc->computed_attrs_offset);
        auto f = desc->computed_attrs()->f;
        for (auto& key : keys) {
            auto ref = f(*address, key);
            if (!ref) raise_AttrNotFound(desc, key);

            ToTreeTraversal child;
            child.r = &object.emplace_back_expect_capacity(key, Tree()).second;
            child.follow_computed_attr<&ToTreeTraversal::visit>(
                *this, ref, f, key, AccessMode::Read
            );
        }
        new (r) Tree(move(object));
    }

    NOINLINE
    void use_elems (const ElemsDcrPrivate* elems) {
        auto len = elems->chop_flag(AttrFlags::Invisible);
        auto array = UniqueArray<Tree>(Capacity(len));
        for (uint i = 0; i < len; i++) {
            auto acr = elems->elem(i)->acr();
            ToTreeTraversal child;
            child.r = &array.emplace_back_expect_capacity(Tree());
            child.follow_elem<&ToTreeTraversal::visit>(
                *this, acr, i, AccessMode::Read
            );
            child.r->flags |= child.acr->tree_flags();
        }
        new (r) Tree(move(array));
    }

    NOINLINE
    void use_computed_elems (const Accessor* length_acr) {
         // TODO: merge the lambdas in this and use_contiguous_elems
        usize len;
        length_acr->read(*address,
            CallbackRef<void(Mu& v)>(len, [](usize& len, Mu& v){
                len = reinterpret_cast<const usize&>(v);
            })
        );
        auto array = UniqueArray<Tree>(Capacity(len));
        expect(desc->computed_elems_offset);
        auto f = desc->computed_elems()->f;
        for (usize i = 0; i < len; i++) {
            auto ref = f(*address, i);
            if (!ref) raise_ElemNotFound(desc, i);
            ToTreeTraversal child;
            child.r = &array.emplace_back_expect_capacity();
            child.follow_computed_elem<&ToTreeTraversal::visit>(
                *this, ref, f, i, AccessMode::Read
            );
        }
        new (r) Tree(move(array));
    }

    NOINLINE
    void use_contiguous_elems (const Accessor* length_acr) {
        usize len;
        length_acr->read(*address,
            CallbackRef<void(Mu& v)>(len, [](usize& len, Mu& v){
                len = reinterpret_cast<const usize&>(v);
            })
        );
        auto array = UniqueArray<Tree>(Capacity(len));
         // If len is 0, don't even bother calling the contiguous_elems
         // function.  This shortcut isn't really needed for computed_elems.
        if (len) {
            expect(desc->contiguous_elems_offset);
            auto f = desc->contiguous_elems()->f;
            auto ptr = f(*address);
             // TODO: move this below call
            auto child_desc = DescriptionPrivate::get(ptr.type);
            for (usize i = 0; i < len; i++) {
                ToTreeTraversal child;
                child.r = &array.emplace_back_expect_capacity();
                child.follow_contiguous_elem<&ToTreeTraversal::visit>(
                    *this, ptr, f, i, AccessMode::Read
                );
                ptr.address = (Mu*)((char*)ptr.address + child_desc->cpp_size);
            }
        }
        new (r) Tree(move(array));
    }

    NOINLINE
    void use_delegate (const Accessor* acr) {
        ToTreeTraversal child;
        child.r = r;
        child.follow_delegate<&ToTreeTraversal::visit>(*this, acr, AccessMode::Read);
        child.r->flags |= child.acr->tree_flags();
    }

///// ERRORS

    [[noreturn, gnu::cold]] NOINLINE
    void fail () {
        if (desc->values()) {
            raise(e_ToTreeValueNotFound, cat(
                "No value for type ", Type(desc).name(),
                " matches the item's value"
            ));
        }
        else raise(e_ToTreeNotSupported, cat(
            "Item of type ", Type(desc).name(), " does not support to_tree"
        ));
    }

     // NOINLINE this so its stack requirements don't get applied to visit()
    [[gnu::cold]] NOINLINE
    bool wrap_exception () {
        if (diagnostic_serialization) {
            new (r) Tree(std::current_exception());
            return true;
        }
        else return false;
    }

     // TODO: delet this
    [[noreturn, gnu::cold]] NOINLINE
    void raise_KeysTypeInvalid(Type keys_type) {
        raise(e_KeysTypeInvalid, cat(
            "Item of type ", Type(desc).name(),
            " gave keys() type ", keys_type.name(),
            " which does not serialize to an array of strings"
        ));
    }
};

} using namespace in;

Tree item_to_tree (const AnyRef& item, LocationRef loc) {
    return ToTreeTraversal::start(item, loc);
}

DiagnosticSerialization::DiagnosticSerialization () {
    diagnostic_serialization += 1;
}
DiagnosticSerialization::~DiagnosticSerialization () {
    expect(diagnostic_serialization > 0);
    diagnostic_serialization -= 1;
}

} using namespace ayu;
