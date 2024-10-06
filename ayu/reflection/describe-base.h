 // This is the interface for describing types to AYU.
 //
 // A type can be described to AYU by declaring a description with the
 // AYU_DESCRIBE macro.  Here's an example of its usage.
 //
 //     AYU_DESCRIBE(myns::MyClass,
 //         attrs(
 //             attr("MyBase", base<MyBase>(), include),
 //             attr("stuff", &MyClass::stuff, optional),
 //             attr("cousin", value_funcs<OtherClass*>(
 //                  [](const MyClass& v){ return v.get_cousin(); },
 //                  [](MyClass& v, OtherClass* m){ v.set_cousin(m); }
 //             )
 //         )
 //     )
 //
 // AYU_DESCRIBE descriptions must be declared in the global namespace.  For
 // non-template types, you should declare them in the .cpp file associated with
 // your class (or a nearby .cpp file).
 //
 // The first parameter to AYU_DESCRIBE is the type name as you wish it to
 // appear in data files.  It's recommended to fully qualify all namespaces even
 // if you have "using namespace" declarations nearby, because the type name
 // will be stringified by the macro exactly the way it appears.
 //
 // All later parameters to AYU_DESCRIBE must be descriptors, which are
 // documented later in this file under various sections.  Some of the
 // descriptors take accessors, which define how to read and write a particular
 // property of an item.  All functions given to descriptors and accessors
 // should return the same results for the same items, or undesired behavior may
 // occur.
 //
 // It is possible to declare template ayu descriptions, though it is
 // necessarily more complicated.  It requires you to manually specify a
 // function to generate the type name, and the descriptor and accessor
 // functions must be preceded with desc:: because of C++'s name lookup rules in
 // templates.  Also, several of the descriptor and accessor functions must have
 // the "template" keyword added to help out the C++ parser.  See
 // describe-standard.h for some examples of template descriptions.
 //
 // The list of descriptors passed to AYU_DESCRIBE may be empty, in which case
 // the type cannot be serialized or deserialized, but it can still be used with
 // AnyRef and AnyVal, etc.

#pragma once

#include <type_traits>

#include "../common.h"
#include "../data/tree.h"
#include "accessors.internal.h"
#include "anyref.h"
#include "descriptors.internal.h"

namespace ayu {

template <class T>
struct _AYU_DescribeBase {
    ///// GENERAL-PURPOSE DESCRIPTORS
     // Specifies the name of the type, as it will appear in serialized strings.
     // You do not need to provide this for non-template types, since the
     // AYU_DESCRIBE macro will stringify the type name given to it and use that
     // as the name.  You must provide a name for template types, but you
     // probably want to use computed_name instead.
    static constexpr auto name (StaticString);
     // Generate a name dynamically, which can depend on the names of other
     // types.  This function will only be called once, with the result cached
     // for later accesses.  For usage examples, see describe-standard.h.
    static constexpr auto computed_name (AnyString(* f )());
     // Provides a function to transform an item of this type to an ayu::Tree
     // for serialization.  For most types this should not be needed; for
     // aggregate types you usually want attrs() or elems(), and for scalar
     // types delegate() or values().  For more complex types, however, you can
     // use this and from_tree() to control serialization.
    static constexpr auto to_tree (Tree(* f )(const T&));
     // Provides a function to transform an ayu::Tree into an item of this type
     // for deserialization.  For most types this should not be needed, but it's
     // available for more complex types if necessary.  The type will already
     // have been default-constructed (or constructed by its parent's default
     // constructor).  Deserialization of items without default constructors is
     // not yet implemented.  You may specify from_tree along with attrs and/or
     // elems, but the from_tree process will ignore the attrs and elems and
     // will not recursively call their swizzle or init descriptors.
     //
     // The provided Tree will never be the undefined Tree.
     //
     // It is acceptable to call item_to_tree() instead a to_tree() function,
     // but references in the item passed to item_to_tree() will not be
     // serialized properly.
     //
     // TODO: Add construct_from_tree for types that refuse to be default
     // constructed no matter what.
    static constexpr auto from_tree (void(* f )(T&, const Tree&));
     // This is similar to from_tree.  The difference is that after this
     // function is called, deserialization will continue with any other
     // applicable descriptors.  The use case for this is for polymorphic types
     // that need to inspect the tree to know how to allocate their storage, but
     // after that will use delegate() with a more concrete type.
    static constexpr auto before_from_tree (void(* f )(T&, const Tree&));
     // If your type needs extra work to link it to other items after
     // from_tree() has been called on all of them, use this function.  As an
     // example, this is used for pointers so that they can point to other
     // items after those items have been properly constructed.  This is not
     // needed for most types.
     //
     // It is acceptable to call item_from_tree() inside a from_tree function,
     // but the inner call to item_to_tree() will not be able to deserialize
     // references properly.
     //
     // For compound types (types with attributes or elements), this will be
     // called first on all the child items in order, then on the parent item.
     //
     // Two things to be aware of:
     //   - If this item is in an optional attr or elem, and that attr or elem
     //     is not assigned in the from_tree operation, then swizzle will not be
     //     called on it or its child items.
     //   - If this item is in an included attr, the tree passed to swizzle will
     //     be the tree provided to the outer item that includes this one, so
     //     the tree may have more attributes than you expect.
    static constexpr auto swizzle (void(* f )(T&, const Tree&));
     // If your type has an extra step needed to complete its initialization
     // after from_tree() and swizzle(), use this function.  As an example, you
     // can have a window type which sets all its parameters using attrs(), and
     // then calls a library function to open the window in init().
     //
     // Init functions will be called in descending priority order.  If init
     // functions have the same priority, they will be called first on child
     // items in order, then on parent items.
     //
     // Be aware that an optional attr or elem will not have init called on it
     // or its child items if it is not provided with a value in the from_tree
     // operation.
     //
     // There is not currently a way to have multiple inits of different
     // priorities on the same type.
     //
     // If the init function causes more items to be deserialized (by
     // autoloading a resource, for instance), all currently queued init
     // operations will run before the new items' init operations, regardless of
     // priority.
    static constexpr auto init (void(* f )(T&), double priority = 0);
     // Make this type behave like another type.  `accessor` must be the result
     // of one of the accessor functions in the ACCESSOR section below.  If both
     // delegate() and other descriptors are specified, some behaviors may be
     // overridden by those other descriptors.  See serialization.h for details
     // about which descriptors override delegate() during which operations.
    template <class Acr>
    static constexpr auto delegate (const Acr& accessor);
     // Specify custom behavior for default construction.  You shouldn't need
     // to use this unless for some reason the type's default constructor is not
     // visible where you're declaring the AYU_DESCRIPTION.  The function will
     // be passed a void* pointing to an allocated buffer with sizeof(T) and
     // alignof(T), and must construct an object of type T, such as by using
     // placement new.
    static constexpr auto default_construct (void(* f )(void*));
     // Specify custom behavior for destruction, in case the item's destructor
     // is not visible from here.  You should destroy the pointed-to object, but
     // do not delete/free it.  It will be deallocated automatically.
    static constexpr auto destroy (void(* f )(T*));

     // Specify flags for this type specifically.  Currently there's only one
     // supported flag:
     //   - no_refs_to_children: Forbid other items from referencing child items
     //     of this type.  This allows the reference-to-location system to skip
     //     scanning this item.
     // supported is no_refs_to_children, which forbids other items from
     // referencing child items of this item.  This
    static constexpr auto flags (in::TypeFlags);

    ///// DESCRIPTORS FOR ENUM-LIKE TYPES

     // You can use this for enum-like types to provide specific representations
     // for specific values.  All arguments to values(...) must be one of:
     //   - value(NAME, VALUE), where NAME can be a string, an integer, a
     //   double, a bool, or null; and VALUE is a constexpr value of this type.
     //   - value_ptr(NAME, VALUE), where NAME is as above, and VALUE is a
     //   pointer to a (possibly non-constexpr) value of this type.
     //
     // When serializing, the current item will be compared to each VALUE using
     // operator==, and if it matches, serialized as NAME.  If no values match,
     // serialization will continue using other descriptors if available, or
     // throw NoNameForValue if there are none.
     //
     // When deserializing, the provided Tree will be compared to each NAME, and
     // if it matches, the current item will be set to VALUE using operator=.
     // If no names match, deserialization will continue using other descriptors
     // if available, or throw NoValueForName if there are none.
     //
     // Using this, you can provide names for specific values of more complex
     // types.  For instance, for a matrix item, you can provide special names
     // like "id" and "flipx" that refer to specific matrixes, and still allow
     // an arbitrary matrix to be specified with a list of numbers.
    template <class... Values>
        requires (requires (T v) { v == v; v = v; })
    static constexpr auto values (Values&&... vs);
     // This is just like values(), but will use the provided compare and assign
     // functions instead of operator== and operator=, so this type doesn't have
     // to have those operators defined.
    template <class... Values>
    static constexpr auto values_custom (
        bool(* compare )(const T&, const T&),
        void(* assign )(T&, const T&),
        Values&&... vs
    );
     // Specify a named value for use in values(...).  The value must be
     // constexpr copy or move constructible.
    template <class N>
        requires (requires (T&& v) { T(move(v)); })
    static constexpr auto value (const N& name, T&& value);
    template <class N>
        requires (requires (const T& v) { T(v); })
    static constexpr auto value (const N& name, const T& value);
     // Specify a named value for use in values(...).  The value must be a
     // pointer to an item of this type, which doesn't have to be constexpr, but
     // it must be initialized before you call any AYU serialization functions.
    template <class N>
    static constexpr auto value_ptr (const N& name, const T* value);

    ///// DESCRIPTORS FOR OBJECT-LIKE TYPES

     // Specify a list of attributes for this item to behave like an object with
     // a fixed set of attributes.  All arguments to this must be calls to
     // attr().  The attribute list may be empty, in which case the item will be
     // serialized as {}.  Attrs will be deserialized in the order they're
     // specified in the description, not in the order they're provided in the
     // Tree.
    template <class... Attrs>
    static constexpr auto attrs (Attrs&&... as);
     // Specify a single attribute for an object-like type.  When serializing,
     // `key` will be used as the attribute's key, and `accessor`'s read
     // operation will be used to get the attribute's value.  When
     // deserializing, if the attribute with the given key is provided in the
     // Tree, its value will be passed to `accessor`'s write operation.
     // `accessor` must be the output of one of the accessor functions (see the
     // ACCESSORS section below), or a pointer-to-data-member as a shortcut for
     // the member() accessor.  Each attr can also take the following flags,
     // combined with operator|.
     //   - optional: This attribute does not need to be provided when
     //     deserializing.  If it is not provided, `accessor`'s write operation
     //     will not be called (normally AttrMissing would be thrown), and
     //     swizzle and init will not be called on this item or any of its
     //     children.
     //   - include: When serializing, `key` will be ignored and this
     //     attribute's attributes will be merged with this item's attributes
     //     (and if any of those attributes have include specified, their
     //     attributes will also be merged in).  When deserializing, the Tree
     //     may either ignore inheritance and provide this attribute with `key`,
     //     or it may provide all of this attribute's attributes directly
     //     without `key`.  Cannot be combined with optional.
     //   - invisible: This attribute will not be read when serializing, but it
     //     will still be written when deserializing (unless it's also optional
     //     or ignored, which it probably should be).  If your attribute has a
     //     readonly accessor, you probably want to make it invisible; otherwise
     //     it will make the whole item readonly.
     //   - ignored: This attribute will not be written when deserializing, but
     //     it will still be read when serializing (unless it's also invisible,
     //     which it probably should be).  Implies optional.  Use this if you
     //     have an obsolete attribute that no longer has meaning.
     //   - collapse_optional: Only for item types that serialize to an array of
     //     0 or 1 elements (such as std::optional and std::unique_ptr).  An
     //     empty array corresponds to the attribute being entirely missing from
     //     the object, and an array of one element corresponds to the
     //     attribute's value being that one element.  In other words,
     //     { // without collapse_optional
     //         opt_present: [foobar]
     //         opt_absent: []
     //     }
     //     { // with collapse_optional
     //         opt_present: foobar
     //     }
     //     If the item serializes to an non-array or an array of more than one
     //     element, an exception will be thrown.  This flag cannot be combined
     //     with optional or include.
     // TODO: Reject multiple attrs with the same name.
    template <class Acr>
    static constexpr auto attr (
        StaticString key,
        const Acr& accessor,
        in::AttrFlags = {}
    );
     // Same as attr(), but with an extra parameter that specifies a default
     // value.  This parameter is anything that can be converted to a Tree,
     // similar to the name parameter of values.  When serializing, if the
     // serialized attribute's value is equal to this Tree, it will be left out
     // of the object, and when deserializing, if the attribute is left out of
     // the object, it will be deserialized from this Tree.
     //
     // Because you can't create dynamically-allocated storage at runtime, to
     // make the default value a non-empty array or object, you need to declare
     // an array at global scope and pass that in as a StaticArray<Tree> or
     // StaticArray<TreePair>.
    template <class Acr, class Default>
        requires (requires (const Default& def) { Tree(def); })
    static constexpr auto attr_default (
        StaticString key,
        const Acr& accessor,
        const Default& default_value,
        in::AttrFlags = {}
    );
     // Use this for items that may have a variable number of attributes.
     // `accessor` must be the output of one of the accessor functions (see
     // ACCESSORS), and its child type must be uni::AnyArray<uni::AnyString>.
     // Writing to this accessor may clear the contents of this item.
     //
     // During serialization, the list of keys will be determined with
     // `accessor`'s read operation, and for each key, the attribute's value
     // will be set using the computed_elems() descriptor.
     //
     // During deserialization, `accessor`'s write operation will be called with
     // the list of keys provided in the Tree, and it should throw
     // MissingAttr if it isn't given an attribute it needs or
     // UnwantedAttr if it's given an attribute it doesn't accept.  If
     // `accessor` is a readonly accessor, then instead its `read` operation
     // will be called, and the list of provided keys must match exactly or an
     // exception will be thrown.  It is acceptable to ignore the provided list
     // of keys and instead clear the item and later autovivify attributes given
     // to computed_attrs().
     //
     // If keys() is present, computed_attrs() must also be present, and attrs()
     // must not be present.
    template <class Acr>
    static constexpr auto keys (const Acr& accessor);
     // Provide a way to read or write arbitrary attributes.  The function is
     // expected to return an ayu::AnyRef corresponding to the attribute with
     // the given key.  You can create that AnyRef any way you like, such as by
     // using a pointer to the child item, or by using a pointer to the parent
     // item plus an accessor (see ACCESSORS).  If the parent item has no
     // attribute with the given key, you should return an empty or null AnyRef.
     //
     // This may be called with a key that was not in the output of the `keys`
     // accessor.  If that happens, you should return an empty AnyRef (or
     // autovivify if you want).
     //
     // Be careful not to return a AnyRef to a temporary and then use that
     // AnyRef past the temporary's lifetime.  For AYU serialization functions,
     // the AnyRef will only be used while the serialization function is
     // running, or while a KeepLocationCache object is active.  But if you keep
     // the AnyRef yourself by doing, say,
     //     ayu::AnyRef ref = ayu::AnyRef(&object)["foo"];
     // then it's as if you had written something like
     //     Foo& foo = object.get_ref_to_foo();
     // and it's your responsibility not to keep the AnyRef around longer than
     // the referred item's lifetime.
     //
     // If computed_attrs() is present, keys() must also be present, and attrs()
     // must not be present.
    static constexpr auto computed_attrs (AnyRef(* f )(T&, const AnyString&));

    ///// DESCRIPTORS FOR ARRAY-LIKE TYPES

     // Provide a list of elements for this type to behave like a fixed-size
     // array.  All arguments to this must be calls to elem().  The element list
     // may be empty, in which case this item will be serialized as [].
     //
     // Elems are deserialized in order starting at index 0, so it is acceptable
     // to have the first elem clear the contents of the object when written to,
     // in anticipation of the other elems being written.  AnyVal does this,
     // for instance, because its first element is its type, and changing the
     // type necessitates clearing its contents.
     //
     // If you specify both attrs() and elems(), then the type can be
     // deserialized from either an object or an array, and will be serialized
     // using whichever of attrs() and elems() was specified first.
    template <class... Elems>
    static constexpr auto elems (Elems&&... es);
     // Provide an individual element accessor.  `accessor` must be one of the
     // accessors in the ACCESSORS section or a pointer-to-data-member as a
     // shortcut for the member() accessor.  `flags` can be 0 or any |ed
     // combination of:
     //   - optional: This element does not need to be provided when
     //     deserializing.  If it is not provided, `accessor`'s write operation
     //     will not be called (normally LengthRejected would be thrown).  All
     //     optional elements must be on the end of the elems list.
     //   - include: Unlike with attrs, this doesn't do much; all it does is
     //     allow casting between this item and the element.  Earlier versions
     //     of this library allowed included elements to be flattened into the
     //     array representation of the parent item, but the behavior and
     //     implementation were unbelievably complicated, all just to save a few
     //     square brackets.
     //   - invisible: This elem will not be serialized during the to_tree
     //     operation.  You probably want optional or ignored on this elem too.
     //     There can't be any non-invisible elems following the invisible
     //     elems.
     //   - ignored: This elem will not be written during the from_tree
     //     operation.  If any elem has the ignored flag, all elems after it
     //     must also have the ignored flag.
    template <class Acr>
    static constexpr auto elem (
        const Acr& accessor, in::AttrFlags = {}
    );
    template <class Acr>
     // Use this for array-like items of variable length (or fixed-size items
     // with very long length).  The accessor must have a child type of either
     // u32 or u64 (one of which is usize aka size_t).
     // Regardless of the type, its returned value cannot be more than the max
     // array size, 0x7fffffff or 2,147,483,647.
     //
     // Writing to this accessor may clear the contents of the item.
     //
     // When serializing, the length of the resulting array Tree will be
     // determined by calling `accessor`s read method.
     //
     // When deserializing, the `accessor`'s write operation will be called with
     // the length of the provided array Tree, and it should throw
     // WrongLength if it doesn't like the provided length.  If `accessor` is
     // readonly, then instead its read operation will be called, and the
     // provided array Tree's length must match its output exactly or
     // WrongLength will be thrown.
     //
     // If length() is present, computed_elems() or contiguous_elems() must also
     // be present, and elems() must not be present.
    static constexpr auto length (const Acr& accessor);
     // Use this to provide a way to read and write elements at arbitrary
     // indexes.  The return value must be an ayu::AnyRef, which can be created
     // any way you like, including by using an accessor.
     //
     // This might be called with an out-of-bounds index.  If that happens, you
     // should return an empty or null AnyRef.
     //
     // Make sure not to return a AnyRef to a temporary and then keep that
     // AnyRef beyond the temporary's lifetime.  See also computed_attrs.
     //
     // If computed_elems() is present, length() must also be present, and
     // elems() and contiguous_elems() must not be present.
    static constexpr auto computed_elems (AnyRef(* f )(T&, u32));
     // Use this for objects that have elements of identical laid out
     // sequentially in memory.  The provided function must return an AnyPtr to
     // the 0th element, and each subsequent element must be sizeof(Element)
     // bytes after the next one, for a total number of elements equal to
     // whatever is read from or written to the accessor passed to length().
     //
     // If the length is 0, this may or may not be called.  You're allowed to
     // return null if the length is 0, but must not return null otherwise.
     //
     // The memory range must not be invalidated by:
     //   - Reading or writing any items that would come after this one in a
     //     serialization operation, including child elems of this item and
     //     sibling items that are ordered after this one.
     //   - Any swizzle or init operations that could be performed in the same
     //     serialization operation.
     // It may be (and probably will be) invalidated by:
     //   - Writing to the length() accessor of this item.
     //   - Writing to length() or keys() of any parent items of this one.
     //
     // If contiguous_elems() is present, length() must also be present, and
     // elems() and computed_elems() must not be present.
    static constexpr auto contiguous_elems (AnyPtr(* f )(T&));

    ///// ACCESSORS
     // Accessors are internal types that are the output of the functions below.
     // They each have two associated types:
     //   - Parent type: The type of the item that the accessor is applied to
     //     (that's the type of the AYU_DESCRIBE block you're currently in).
     //   - Child type: The type of the item that this accessor points to.
     // Accessors have up to four operations that they support:
     //   - read: Read the value of the child item from the parent item.  All
     //     accessors support this operation.
     //   - write: Write a value to the child item through the parent item.  If
     //     an accessor is readonly, it does not support this operation.
     //   - address: Get the memory address of a child item from the parent
     //     item.  If an accessor supports this operation, various serialization
     //     operations will be much more efficient, and pointers can be
     //     serialized and deserialized which point to the child item.
     //   - reverse_address: Get the memory address of a parent item from a
     //     child item.  This operation is only used for downcasting, and very
     //     few accessors support it.
     // In addition, accessors can take these flags:
     //   - readonly: Make this accessor readonly and disable its write
     //     operation.  If an accessor doesn't support the write operation, it
     //     is readonly by default and this flag is ignored.  If you have an
     //     attr or elem with a readonly accessor, the attr or elem should be
     //     flagged with invisible|optional or invisible|ignored, otherwise the
     //     parent item will not survive a round-trip serialize-deserialize.
     //     This doesn't matter if the parent item is only ever going to be
     //     serialized and never deserialized.
     //   - prefer_hex: The item this accessor points to prefers to be
     //     serialized in hexadecimal format if it's a number.
     //   - prefer_compact: The item this accessor points to prefers to be
     //     serialized compactly (for arrays, objects, and strings).
     //   - prefer_expanded: The item this accessor points to prefers to be
     //     serialized in expanded multi-line form (for arrays, objects, and
     //     strings).  The behavior is unspecified if both prefer_compact and
     //     prefer_expanded are given, and if multiple accessors are followed to
     //     reach an item, which accessor's prefer_* flags are used is
     //     unspecified.
     //   - pass_through_addressable: Normally you can only take the address of
     //     a child item if its parent is also addressable, but if the parent
     //     item's accessor has pass_through_addressable, then instead you can
     //     take its address if the parent's parent is addressable (or if the
     //     parent's parent also has pass_through_addressable, it's parent's
     //     parent's parent).  If you misuse this, you can potentially leave
     //     dangling pointers around, risking memory corruption, so be careful.
     //     This is intended to be used for reference-like proxy items, which
     //     are generated temporarily but refer to non-temporary items.
     //   - unaddressable: Consider items accessed through this accessor to be
     //     unaddressable, even if they look like they should be addressable.
     //     You shouldn't need to use this unless the parent has
     //     pass_through_addressable, or for some reason you're returning
     //     AnyRefs to items with unstable addresses in computed_attrs or
     //     computed_elems.

     // This accessor gives access to a non-static data member of a class by
     // means of a pointer-to-member.  This accessor will be addressable and
     // reverse_addressable.  Obviously this is only available for class and
     // union C++ types, and will cause a compile-time error when used on
     // scalar C++ types.
     //
     // For attr() and elem(), you can pass the pointer-to-member directly and
     // it's as if you had used member().  If you need to specify any accessor
     // flags you still have to use member().
     //
     // If the class's data members are private but you still want to access
     // them through this, you can declare the AYU description as a friend
     // class by saying
     //     class MyClass {
     //         ...
     //         AYU_FRIEND_DESCRIBE(MyClass)
     //     };
    template <class T2, class M>
    static constexpr auto member (
        M T2::* mp, in::AcrFlags = {}
    );
     // Give access to a const non-static data member.  This accessor will be
     // readonly, and is addressable and reverse_addressable.
    template <class T2, class M>
    static constexpr auto const_member (
        const M T2::* mp, in::AcrFlags = {}
    );
     // Give access to a base class.  This can be any type where a C++ reference
     // to the derived class can be implicitly cast to a reference to the base
     // class, and a reference to the base class can be static_cast<>()ed to a
     // reference to the derived class.  This accessor is addressable and
     // reverse_addressable.
    template <class B>
        requires (requires (T* t, B* b) { b = t; t = static_cast<T*>(b); })
    static constexpr auto base (
        in::AcrFlags = {}
    );
     // Give access to a child item by means of a function that returns a
     // non-const C++ reference to the item.  This accessor is addressable, but
     // with the natural caveat that the address must not be used after the
     // referenced item's lifetime expires.  In the case of AYU serialization
     // functions, the address will only be used while the serialization
     // function is still running or while a KeepLocationCache object is active,
     // but if you take the address yourself using, say,
     //     Foo* ptr = AnyRef(&object)["foo"];
     // and the AYU_DESCRIPTION of object's type has an attr "foo" with a
     // ref_func, then it's as if you said something like
     //     Foo* ptr = &object.get_foo_ref();
     // and it's your responsibilty not to keep the pointer around longer than
     // the reference is valid.
    template <class M>
    static constexpr auto ref_func (
        M&(* f )(T&), in::AcrFlags = {}
    );
     // Just like ref_func, but creates a readonly accessor.  Just like with
     // ref_func, be careful when returning a reference to a temporary.
    template <class M>
    static constexpr auto const_ref_func (
        const M&(* f )(const T&),
        in::AcrFlags = {}
    );
     // This makes a read-write accessor based on two functions, one of which
     // returns a const ref to the child, and the other of which takes a const
     // ref to a child and writes a copy to the parent.  This accessor is not
     // addressable.  If possible, it's better to use member() than this.
    template <class M>
    static constexpr auto const_ref_funcs (
        const M&(* g )(const T&),
        void(* s )(T&, const M&),
        in::AcrFlags = {}
    );
     // This makes a readonly accessor from a function that returns a child item
     // by value.  It is not addressable.
    template <class M>
        requires (requires (M m) { M(move(m)); })
    static constexpr auto value_func (
        M(* f )(const T&),
        in::AcrFlags = {}
    );
     // This makes a read-write accessor from two functions that read and write
     // a child item by value.  It is not addressable.
    template <class M>
        requires (requires (M m) { M(move(m)); })
    static constexpr auto value_funcs (
        M(* g )(const T&),
        void(* s )(T&, M),
        in::AcrFlags = {}
    );
     // This makes a read-write accessor from two functions, the first of which
     // returns a child item by value, the second of which takes a child item by
     // const reference and writes a copy to the parent.  This is what you want
     // if the child item is something like a std::vector that's generated on
     // the fly.
    template <class M>
        requires (requires (M m) { M(move(m)); })
    static constexpr auto mixed_funcs (
        M(* g )(const T&),
        void(* s )(T&, const M&),
        in::AcrFlags = {}
    );
     // This makes an accessor to any child item such that the parent and child
     // types can be assigned to eachother with operator=.  It is not
     // addressable.
     //
     // I'm not sure how useful this is since you can just use value_funcs
     // instead, but here it is.
    template <class M>
        requires (requires (T t, M m) { t = m; m = t; })
    static constexpr auto assignable (
        in::AcrFlags = {}
    );
     // This makes a readonly accessor which always returns a constant.  The
     // provided constant must be constexpr copy or move constructible.  This
     // accessor is not addressable, though theoretically it could be made to
     // be.
    template <class M>
        requires (requires (M m) { M(move(m)); })
    static constexpr auto constant (
        M&& v, in::AcrFlags = {}
    );
     // Makes a readonly accessor which always returns a constant.  The pointed-
     // to constant does not need to be constexpr or even copy-constructible,
     // but it must be initialized before calling any AYU serialization
     // functions.  This accessor is addressable.
    template <class M>
    static constexpr auto constant_ptr (
        const M* p, in::AcrFlags = {}
    );
     // Like constant(), but provides read-write access to a variable which is
     // embedded in the accessor with move().  This accessor is not
     // constexpr, so it cannot be used directly in an AYU_DESCRIBE block, and
     // can only be used inside an computed_attrs or computed_elems.  It is not
     // addressable.  There is no corresponding variable_pointer accessor
     // because if you're in an computed_attrs or computed_elems, you can just
     // convert the pointer directly to an ayu::AnyRef instead of using an
     // accessor.
     //
     // This is intended to be used for proxy types along with
     // pass_through_addressable.
    template <class M>
        requires (requires (M m) { M(move(m)); m.~M(); })
    static auto variable (
        M&& v, in::AcrFlags = {}
    );
     // An accessor that gives access to a child item by means of an ayu::AnyRef
     // instead of a C++ reference.  This and anyptr_func are the only accessors
     // whose child type can vary depending on the parent item it's applied to.
     //
     // This accessor is considered unaddressable, even if the returned anyref
     // is addressable.  Use anyptr_func to give access to an arbitrarily-typed
     // addressable item.
     //
     // Unlike computed_attrs and computed_elems, you should not return an empty
     // AnyRef from this function, or you may get null pointer derefs down
     // the line.
     //
     // If the returned AnyRef was made with an accessor that has different
     // flags than this one, which flags are used is Unspecified Behavior.
    static constexpr auto anyref_func (
        AnyRef(* f )(T&), in::AcrFlags = {}
    );
     // An accessor that gives access to a child item through an ayu::AnyPtr.
     // Like anyref_func but it's addressable.  Don't return empty or null from
     // this.
    static constexpr auto anyptr_func (
        AnyPtr(* f )(T&), in::AcrFlags = {}
    );

    ///// METHOD ACCESSORS
     // These are syntax sugar for the _func(s) accessors which use methods
     // instead of functions.  They aren't first-class citizens because C++'s
     // method pointers are kinda scuffed.  Example usage:
     // value_methods<
     //     usize, &std::vector<T>::size, &std::vector<T>::resize
     // >()
    using T3 = std::conditional_t<
        std::is_class_v<T> || std::is_union_v<T>,
        T, Mu
    >;
     // ref_method
    template <class M, M&(T3::* get )()>
    static constexpr auto ref_method (
        in::AcrFlags flags = {}
    ) {
        return ref_func<M>(
            [](const T& v) -> M& { return (v.*get)(); }, flags
        );
    }
     // const_ref_method
    template <class M, const M&(T3::* get )()const>
    static constexpr auto const_ref_method (
        in::AcrFlags flags = {}
    ) {
        return const_ref_func<M>(
            [](const T& v) -> const M& { return (v.*get)(); }, flags
        );
    }
     // const_ref_methods
    template <class M, const M&(T3::* get )()const, void(T3::* set )(const M&)>
    static constexpr auto const_ref_methods (
        in::AcrFlags flags = {}
    ) {
        return const_ref_funcs<M>(
            [](const T& v) -> const M& { return (v.*get)(); },
            [](T& v, const M& m){ (v.*set)(m); },
            flags
        );
    }
     // value_method
    template <class M, M(T3::* get )()const>
    static constexpr auto value_method (
        in::AcrFlags flags = {}
    ) {
        return value_func<M>(
            [](const T& v) -> M { return (v.*get)(); }, flags
        );
    }
     // value_methods
    template <class M, M(T3::* get )()const, void(T3::* set )(M)>
    static constexpr auto value_methods (
        in::AcrFlags flags = {}
    ) {
        return value_funcs<M>(
            [](const T& v) -> M { return (v.*get)(); },
            [](T& v, M m){ (v.*set)(m); },
            flags
        );
    }
     // mixed_methods
    template <class M, M(T3::* get )()const, void(T3::* set )(const M&)>
    static constexpr auto mixed_methods (
        in::AcrFlags flags = {}
    ) {
        return mixed_funcs<M>(
            [](const T& v) -> M { return (v.*get)(); },
            [](T& v, const M& m){ (v.*set)(m); },
            flags
        );
    }
     // anyref_method.  I doubt you'll ever need this but here it is.
    template <AnyRef (T3::* get )()>
    static constexpr auto anyref_method (
        in::AcrFlags flags = {}
    ) {
        return anyref_func(
            [](const T& v) -> AnyRef { return (v.*get)(); }, flags
        );
    }
     // And an overload for init() (a descriptor, not an accessor, but this
     // seems convenient, as a lot of class-like types have a method for this.
    template <void(T3::* m )()>
    static constexpr auto init (double priority = 0) {
        return init([](T& v){ (v.*m)(); }, priority);
    }

    ///// FLAGS AND INTERNAL STUFF

    static constexpr in::TypeFlags no_refs_to_children = in::TypeFlags::NoRefsToChildren;
    static constexpr in::AttrFlags optional = in::AttrFlags::Optional;
    static constexpr in::AttrFlags include = in::AttrFlags::Include;
    static constexpr in::AttrFlags invisible = in::AttrFlags::Invisible;
    static constexpr in::AttrFlags ignored = in::AttrFlags::Ignored;
    static constexpr in::AttrFlags collapse_optional = in::AttrFlags::CollapseOptional;
    static constexpr in::AcrFlags readonly = in::AcrFlags::Readonly;
    static constexpr in::AcrFlags prefer_hex = in::AcrFlags::PreferHex;
    static constexpr in::AcrFlags prefer_compact = in::AcrFlags::PreferCompact;
    static constexpr in::AcrFlags prefer_expanded = in::AcrFlags::PreferExpanded;
    static constexpr in::AcrFlags pass_through_addressable =
        in::AcrFlags::PassThroughAddressable;
    static constexpr in::AcrFlags unaddressable = in::AcrFlags::Unaddressable;
    template <class... Dcrs>
    static constexpr auto _ayu_describe (Dcrs&&... dcrs);
};

} // namespace ayu

#include "describe-base.inline.h"
