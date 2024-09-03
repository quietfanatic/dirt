 // This module contains functions to manipulate compound objects using the
 // traversal system.

#pragma once

#include "../common.h"
#include "location.h"

namespace ayu {

///// OPERATIONS FOR OBJECT-LIKE TYPES
 // These use either the attrs() descriptor or the keys() and attr_func()
 // descriptors.  If neither are available, they use the delegate() descriptor,
 // and if that isn't available, they throw e_AttrsNotSupported.

 // Get a list of the keys in a object-like item.
AnyArray<AnyString> item_get_keys (
    const AnyRef&, LocationRef loc = {}
);
 // Set the keys in an object-like item.  This may clear the entire contents
 // of the item.  If the item only accepts certain attribute keys, this may
 // throw e_AttrMissing or e_AttrRejected.
void item_set_keys (
    const AnyRef&, AnyArray<AnyString>,
    LocationRef loc = {}
);
 // Get an attribute of an object-like item by its key, or empty AnyRef if
 // the attribute doesn't exist.
AnyRef item_maybe_attr (
    const AnyRef&, const AnyString&, LocationRef loc = {});
 // Throws e_ElemNotFound if the attribute doesn't exist.  Guaranteed not to
 // return an empty or null AnyRef.
AnyRef item_attr (const AnyRef&, const AnyString&, LocationRef loc = {});

///// OPERATIONS FOR ARRAY-LIKE TYPES
 // These use either the elems() descriptor or the keys() and elem_func()
 // descriptors.  If neither are available, they use the delegate() descriptor,
 // and if that isn't available, they throw e_ElemsNotSupported.

 // Get the length of an array-like item.
usize item_get_length (const AnyRef&, LocationRef loc = {});
 // Set the length of an array-like item.  This may clear some or all of the
 // contents of the item.  If the item only allows certain lengths, this may
 // throw e_WrongLength.
void item_set_length (
    const AnyRef&, usize, LocationRef loc = {}
);
 // Get an element of an array-like item by its index.  Returns an empty
 // AnyRef if the element doesn't exist.
AnyRef item_maybe_elem (
    const AnyRef&, usize, LocationRef loc = {}
);
 // Throws e_ElemNotFound if the element doesn't exist.  Guaranteed not to return
 // an empty or null AnyRef.
AnyRef item_elem (
    const AnyRef&, usize, LocationRef loc = {}
);

 // The keys() descriptor of an item produced a type other than
 // AnyArray<AnyString>.
constexpr ErrorCode e_KeysTypeInvalid = "ayu::e_KeysTypeInvalid";
 // The set_keys operation (which is also part of the item_from_tree process)
 // failed because a key that the item required was not given.
constexpr ErrorCode e_AttrMissing = "ayu::e_AttrMissing";
 // The set_keys operation (which is also part of the item_from_tree process)
 // failed because a key was given that the item did not accept.
constexpr ErrorCode e_AttrRejected = "ayu::e_AttrRejected";
 // Called item_attr on an item but it didn't have the requested attr.  This
 // can happen in item_to_tree or item_from_tree if an item's keys() descriptor
 // produces or accepts a key that its attr_func() descriptor then rejects.
constexpr ErrorCode e_AttrNotFound = "ayu::e_AttrNotFound";
 // Tried to treat an item like an object that doesn't support it.  This can be
 // thrown by any of the keys or attr operations and also by item_from_tree
 // (when given an object tree for the item).
constexpr ErrorCode e_AttrsNotSupported = "ayu::e_AttrsNotSupported";

 // An item's length accessor returned a type other than usize
constexpr ErrorCode e_LengthTypeInvalid = "ayu::e_LengthTypeRejected";
 // The set_length operation (which is also part of the item_from_tree process)
 // failed because the provided length was not accepted by the item.
constexpr ErrorCode e_LengthRejected = "ayu::e_LengthRejected";
 // Called item_elem on an item but it didn't have the requested elem (the given
 // index was out of bounds).  This can happen in item_to_tree or item_from_tree
 // if an item's elem_func() descriptor doesn't accept all of the indexes its
 // length() descriptor said it would.
constexpr ErrorCode e_ElemNotFound = "ayu::e_ElemNotFound";
 // Tried to treat an item like an array that doesn't support it.  This can be
 // thrown by any of the length or elem operations and also by item_from_tree
 // (when given an array tree for the item).
constexpr ErrorCode e_ElemsNotSupported = "ayu::e_ElemsNotSupported";

 // You might want to use these error types in your descriptions.
[[noreturn, gnu::cold]]
void raise_AttrMissing (Type item_type, const AnyString& key);
[[noreturn, gnu::cold]]
void raise_AttrRejected (Type item_type, const AnyString& key);
[[noreturn, gnu::cold]]
void raise_LengthRejected (Type item_type, usize min, usize max, usize got);

 // These are less likely to be useful, but they're here in case you want them.
[[noreturn, gnu::cold]]
void raise_KeysTypeInvalid (Type item_type, Type got_type);
[[noreturn, gnu::cold]]
void raise_AttrNotFound (Type, const AnyString&);
[[noreturn, gnu::cold]]
void raise_AttrsNotSupported (Type);
[[noreturn, gnu::cold]]
void raise_LengthTypeInvalid (Type item_type, Type got_type);
[[noreturn, gnu::cold]]
void raise_ElemNotFound (Type, usize);
[[noreturn, gnu::cold]]
void raise_ElemsNotSupported (Type);

inline void require_readable_keys (Type t) {
    if (t.remove_readonly() != Type::CppType<AnyArray<AnyString>>()) {
        raise_KeysTypeInvalid(Type(), t);
    }
}
inline void require_writeable_keys (Type t) {
    if (t != Type::CppType<AnyArray<AnyString>>()) {
        raise_KeysTypeInvalid(Type(), t);
    }
}
inline void require_readable_length (Type t) {
    if (t.remove_readonly() != Type::CppType<usize>()) {
        raise_LengthTypeInvalid(Type(), t);
    }
}
inline void require_writeable_length (Type t) {
    if (t != Type::CppType<usize>()) {
        raise_LengthTypeInvalid(Type(), t);
    }
}

} // namespace ayu
