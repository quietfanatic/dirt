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
 // should be idempotent, returning the same results whenever they are called,
 // or else undesired behavior may occur.
 //
 // It is possible to declare descriptions for template classes, though it is
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
#include "access.internal2.h"
#include "anyref.h"
#include "description.internal.h"
#include "type.h"

namespace ayu {

///// TYPES AND CONSTRAINTS

 // The top level arguments of an AYU_DESCRIBE list must match this.
template <class Dcr, class T>
concept DescriptorFor = std::is_base_of_v<in::Descriptor<T>, Dcr>;

 // The arguments to values and values_custom must match this.
template <class Dcr, class T>
concept ValueDcrFor = std::is_base_of_v<in::ValueDcr<T>, Dcr>;

 // The arguments to attrs must match this.
template <class Dcr, class T>
concept AttrDcrFor = std::is_base_of_v<in::AttrDcr<T>, Dcr>;

 // The arguments to elems must match this.
template <class Dcr, class T>
concept ElemDcrFor = std::is_base_of_v<in::ElemDcr<T>, Dcr>;

#if 0 // These are declared elsewhere.

 // This matches an argument to each of delegate, attr, elem, keys, length.
template <class Acr, class From>
concept AccessorFrom = std::is_same_v<typename Acr::AcrFromType, From>;

 // This further constrains the argument to keys and length.
template <class Acr, class To>
concept AccessorTo = std::is_same_v<typename Acr::AcrToType, To>;

 // Constrain both from and to type
template <class Acr, class From, class To>
concept AccessorFromTo = AccessorFrom<Acr, From> && AccessorTo<Acr, To>;

#endif

 // Passed to flags descriptor
using TypeFlags = in::TypeFlags;

 // Passed to accessor constructors
using AcrFlags = in::AcrFlags;

 // Passed to attr and elem
using AttrFlags = in::AttrFlags;

 // Convenience since C's functions parameter syntax is kooky.
template <class F> using Function = F;

template <Describable T>
struct AYU_DescribeBase {

    ///// GENERAL-PURPOSE DESCRIPTORS

     // Specifies the name of the type, as it will appear in serialized strings.
     // You do not need to provide this for non-template types, since the
     // AYU_DESCRIBE macro will stringify the type name given to it and use that
     // as the name.  You must provide a name for template types, but you
     // probably want to use computed_name instead.
    static constexpr
    DescriptorFor<T> auto name (
        StaticString
    );

     // Generate a name dynamically, which can depend on the names of other
     // types.  This function will only be called once, with the result cached
     // for later accesses.  For usage examples, see describe-standard.h.
    static constexpr
    DescriptorFor<T> auto computed_name (
        Function<AnyString()>*
    );

     // Provides a function to transform an item of this type to an ayu::Tree
     // for serialization.  For most types this should not be needed; for
     // aggregate types you usually want attrs() or elems(), and for scalar
     // types delegate() or values().  For more complex types, however, you can
     // use this and from_tree() to control serialization.
    static constexpr
    DescriptorFor<T> auto to_tree (
        Function<Tree(const T&)>*
    );

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
    static constexpr
    DescriptorFor<T> auto from_tree (
        Function<void(T&, const Tree&)>*
    );

     // This is similar to from_tree.  The difference is that after this
     // function is called, deserialization will continue with any other
     // applicable descriptors.  The use case for this is for polymorphic types
     // that need to inspect the tree to know how to allocate their storage, but
     // after that will use delegate() with a more concrete type.
    static constexpr
    DescriptorFor<T> auto before_from_tree (
        Function<void(T&, const Tree&)>*
    );

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
     //   - If this item is in a collapsed/included attr, the tree passed to
     //     swizzle will be the tree provided to the outer item that includes
     //     this one, so the tree may have more attributes than you expect.
    static constexpr
    DescriptorFor<T> auto swizzle (
        Function<void(T&, const Tree&)>*
    );

     // If your type has an extra step needed to complete its initialization
     // after from_tree() and swizzle(), use this function.  As an example, you
     // can have a window type which sets all its parameters using attrs(), and
     // then calls a library function to open the window in init().
     //
     // Init functions will be called in ascending order of their `order`
     // parameter.  If two init functions have the same `order`, they will be
     // called first on child items in serialization order, then on parent
     // items.
     //
     // Be aware that an optional attr or elem will not have init called on it
     // or its child items if it is not provided with a value in the from_tree
     // operation.
     //
     // There is not currently a way to have multiple inits with different
     // orders on the same type.
     //
     // If the init function causes more items to be deserialized (by
     // autoloading a resource, for instance), all currently queued init
     // operations will run before the new items' swizzle or init operations,
     // regardless of order.
    static constexpr
    DescriptorFor<T> auto init (
        Function<void(T&)>*,
        double order = 0
    );

     // Make this type behave like another type.  `accessor` must be the result
     // of one of the accessor functions in the ACCESSOR section below.
     //
     // If both delegate() and other descriptors are specified, some behaviors
     // may be overridden by those other descriptors.
     //
     // Using a delegate descriptor also makes the delegated-to type a candidate
     // for upcasting.  See dynamic_upcast in type.h
    template <AccessorFrom<T> Acr> static constexpr
    DescriptorFor<T> auto delegate (
        const Acr& accessor
    );
     // Shortcut for delegate(member(...))
    template <SameOrBase<T> T2, Describable M> static constexpr
    DescriptorFor<T> auto delegate (
        M T2::* mp
    ) { return delegate(member(mp)); }

     // Specify custom behavior for default construction.  You shouldn't need
     // to use this unless for some reason the type's default constructor is not
     // visible where you're declaring the AYU_DESCRIPTION.  The function will
     // be passed a void* pointing to an allocated buffer with sizeof(T) and
     // alignof(T), and must construct an object of type T, such as by using
     // placement new.
    static constexpr
    DescriptorFor<T> auto default_construct (
        Function<void(void*)>*
    );

     // Specify custom behavior for destruction, in case the item's destructor
     // is not visible from here.  You should destroy the pointed-to object, but
     // do not delete/free it.  It will be deallocated automatically.
    static constexpr
    DescriptorFor<T> auto destroy (
        Function<void(T*)>*
    );

     // Specify flags for this type specifically.  Currently there are one and a
     // half supported flags:
     //   - no_refs_to_children: Forbid other items from referencing recursive
     //     child items of this type.  This item itself can still be referenced.
     //     This allows the reference-to-route scanning system to save time by
     //     skipping this item's children.
     //   - no_refs_from_children: Forbid this item's recursive children from
     //     containing references.  This is not currently enforced, and is for
     //     documentation purposes only.  There are some types which will cause
     //     confusing breakages when they contain references--most notably,
     //     std::set and std::unordered_set.
    static constexpr
    DescriptorFor<T> auto flags (
        TypeFlags
    );

    static constexpr TypeFlags no_refs_to_children = in::TypeFlags::NoRefsToChildren;
    static constexpr TypeFlags no_refs_from_children = in::TypeFlags::NoRefsFromChildren;

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
     // throw ToTreeValueNotFound if there are none.
     //
     // When deserializing, the provided Tree will be compared to each NAME, and
     // if it matches, the current item will be set to VALUE using operator=.
     // If no names match, deserialization will continue using other descriptors
     // if available, or throw FromTreeValueNotFound if there are none.
     //
     // You can also provide names for specific values of more complex types.
     // For instance, for a matrix item, you can provide special names like "id"
     // and "flipx" that refer to specific matrixes, and still allow an
     // arbitrary matrix to be specified with a list of numbers.
     //
     // There cannot be more than 1000 values.  If there are close to that many,
     // you should probably be using a binary search or hash table anyway,
     // instead of the linear search this library provides.
    template <ValueDcrFor<T>... Values> requires (
        requires (T v) { v == v; v = v; }
    ) static constexpr
    DescriptorFor<T> auto values (
        const Values&... value_descriptors
    );

     // This is just like values(), but will use the provided compare and assign
     // functions instead of operator== and operator=, so this type doesn't have
     // to have those operators defined.  The first argument to compare is the
     // item being serialized, and the second value is the value()'s value.
    template <ValueDcrFor<T>... Values> static constexpr
    DescriptorFor<T> auto values_custom (
        Function<bool(const T&, const T&)>* compare,
        Function<void(T&, const T&)>* assign,
        const Values&... value_descriptors
    );

     // Specify a named value for use in values(...).  The value must be
     // constexpr copy constructible.
    template <ConstructsTree N> requires (Copyable<T>) static constexpr
    ValueDcrFor<T> auto value (
        const N& name, const T& val
    );
     // Specify a named value for use in values(...).  The value must be a
     // pointer to an item of this type, which doesn't have to be constexpr, but
     // it must be initialized before you call any AYU serialization functions.
    template <ConstructsTree N> static constexpr
    ValueDcrFor<T> auto value_ptr (
        const N& name, const T* val
    );

    ///// DESCRIPTORS FOR OBJECT-LIKE TYPES

     // Specify a list of attributes for this item to behave like an object with
     // a fixed set of attributes.  All arguments to this must be calls to
     // attr().  The attribute list may be empty, in which case the item will be
     // serialized as {}.  Attrs will be deserialized in the order they're
     // specified in the description, not in the order they're provided in the
     // Tree.
     //
     // There cannot be more than 1000 attrs specified in the description
     // (subattrs of collapsed attrs do not count).  More than that and you'll
     // have to use computed_attrs() instead.
    template <AttrDcrFor<T>... Attrs> static constexpr
    DescriptorFor<T> auto attrs (
        const Attrs&... attr_descriptors
    );

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
     //   - collapse: The value of this attribute must be an object-like type.
     //     When serializing, `key` will be ignored and this attribute's
     //     attributes will be merged with this item's attributes (recursively,
     //     if any of the attribute's attributes also have collapse or include
     //     set).  When deserializing, the Tree may either ignore collapsing and
     //     provide this attribute with `key`, or it may provide all of this
     //     attribute's attributes directly without `key`.  Cannot be combined
     //     with optional.
     //   - castable: Consider this attr a candidate for upcasting (typically
     //     used with the base<>() accessor).
     //   - include: Short for both collapse and castable.
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
     //     with optional or collapse.
     // TODO: Reject multiple attrs with the same name.
    template <AccessorFrom<T> Acr> static constexpr
    AttrDcrFor<T> auto attr (
        StaticString key,
        const Acr& accessor,
        AttrFlags = {}
    );
     // Shortcut for attr("...", member(...))
    template <SameOrBase<T> T2, Describable M> static constexpr
    AttrDcrFor<T> auto attr (
        StaticString key,
        M T2::* mp,
        AttrFlags flags = {}
    ) { return attr(key, member(mp), flags); }

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
     //
     // Yes, it would make more sense for the attribute to be an actual object
     // of the accessor's To type, rather than a Tree-like object, but that
     // would require capturing the type's comparison and assignment operators,
     // which would be either complicated or inefficient.
    template <AccessorFrom<T> Acr, ConstructsTree Default> static constexpr
    AttrDcrFor<T> auto attr_default (
        StaticString key,
        const Acr& accessor,
        const Default& default_tree_value,
        AttrFlags = {}
    );

     // Shortcut for attr_default("...", member(...), ...)
    template <SameOrBase<T> T2, Describable M, ConstructsTree Default>
    static constexpr
    AttrDcrFor<T> auto attr_default (
        StaticString key,
        M T2::* mp,
        const Default& default_tree_value,
        AttrFlags flags = {}
    ) { return attr_default(key, member(mp), default_tree_value, flags); }

     // Flags for attr (and elem).
    static constexpr AttrFlags optional = in::AttrFlags::Optional;
    static constexpr AttrFlags collapse = in::AttrFlags::Collapse;
    static constexpr AttrFlags castable = in::AttrFlags::Castable;
    static constexpr AttrFlags include = collapse | castable;
    static constexpr AttrFlags invisible = in::AttrFlags::Invisible;
    static constexpr AttrFlags ignored = in::AttrFlags::Ignored;
    static constexpr AttrFlags collapse_optional = in::AttrFlags::CollapseOptional;

     // Use this for items that may have a variable number of attributes.
     // `accessor` must be the output of one of the accessor functions (see
     // ACCESSORS), and its child type must be uni::AnyArray<uni::AnyString>.
     // Writing to this accessor may clear the contents of this item.
     //
     // During serialization, the list of keys will be determined with
     // `accessor`'s read operation, and for each key, the attribute's value
     // will be set using the computed_attrs() descriptor.
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
    template <AccessorFrom<T> Acr> requires (
        AccessorTo<Acr, uni::AnyArray<uni::AnyString>>
    ) static constexpr
    DescriptorFor<T> auto keys (
        const Acr& accessor
    );

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
     // Be careful not to return an AnyRef to a temporary and then use that
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
     // The returned AnyRef must not be invalidated by:
     //   - Reading or writing any items that would come after this one in a
     //     serialization operation, including child items of this item and
     //     sibling items that are ordered after this one.
     //   - Any swizzle or init operations that could be performed in the same
     //     serialization operation.
     // It may be invalidated by:
     //   - The before_from_tree() function of this item, if it exists.
     //   - Writing to the keys() accessor of this item (or length() accessor
     //     for computed_elems and contiguous_elems()).
     //   - Writing to sibling items that are ordered before this one.
     //   - The before_from_tree(), length() setter, or keys() setter of any
     //     recursive parent items of this one.
     //   - Running program logic outside of serialization operations.
     // In addition, as long as the AnyRef would still be valid, a second call
     // to the computed_attrs function must return the same or an equivalent
     // AnyRef (equivalent because if this item is unaddressable, its memory
     // address may be different between calls, so the AnyRef doesn't have to be
     // exactly the same, but it must be functionally the same: it must have the
     // same access capabilities, and reading and writing it must have the same
     // effects).
     //
     // If computed_attrs() is present, keys() must also be present, and attrs()
     // must not be present.
    static constexpr
    DescriptorFor<T> auto computed_attrs (
        Function<AnyRef(T&, const AnyString&)>*
    );

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
     //
     // There cannot be more than 1000 elems.  Any more than that and you will
     // have to use computed_elems(), or if possible, contiguous_elems().
    template <ElemDcrFor<T>... Elems> static constexpr
    DescriptorFor<T> auto elems (
        const Elems&... elem_descriptors
    );
     // Provide an individual element accessor.  `accessor` must be one of the
     // accessors in the ACCESSORS section or a pointer-to-data-member as a
     // shortcut for the member() accessor.  `flags` can be 0 or any |ed
     // combination of:
     //   - optional: This element does not need to be provided when
     //     deserializing.  If it is not provided, `accessor`'s write operation
     //     will not be called (normally LengthRejected would be thrown).  All
     //     optional elements must be on the end of the elems list.
     //   - castable: Consider this elem a candidate for upcasting.
     //   - invisible: This elem will not be serialized during the to_tree
     //     operation.  You probably want optional or ignored on this elem too.
     //     There can't be any non-invisible elems following the invisible
     //     elems.
     //   - ignored: This elem will not be written during the from_tree
     //     operation.  If any elem has the ignored flag, all elems after it
     //     must also have the ignored flag.
    template <AccessorFrom<T> Acr> static constexpr
    ElemDcrFor<T> auto elem (
        const Acr& accessor, AttrFlags = {}
    );
     // Shortcut for elem(member(...))
    template <SameOrBase<T> T2, Describable M> static constexpr
    ElemDcrFor<T> auto elem (
        M T2::* mp, AttrFlags flags = {}
    ) { return elem(member(mp), flags); }

     // Use this for array-like items of variable length (or fixed-size items
     // with very long length).  The accessor must have a child type of either
     // u32 or u64 (one of which is usize aka size_t).
     // Regardless of the type, its returned value cannot be more than the max
     // array size of 0x7fffffff or 2,147,483,647.  If you need to work with
     // array-like items larger than that, you should probably be using a binary
     // format.
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
    template <AccessorFrom<T> Acr> requires (
        AccessorTo<Acr, u32> || AccessorTo<Acr, u64>
    ) static constexpr
    DescriptorFor<T> auto length (
        const Acr& accessor
    );

     // Use this to provide a way to read and write elements at arbitrary
     // indexes.  The return value must be an ayu::AnyRef, which can be created
     // any way you like, including by using an accessor.
     //
     // This might be called with an out-of-bounds index.  If that happens, you
     // should return an empty or null AnyRef.
     //
     // The rules for validity and consistency of the returned AnyRef are the
     // same as those for computed_attrs().
     //
     // If computed_elems() is present, length() must also be present, and
     // elems() and contiguous_elems() must not be present.
    static constexpr
    DescriptorFor<T> auto computed_elems (
        Function<AnyRef(T&, u32)>*
    );

     // Use this for objects that have elements of identical laid out
     // sequentially in memory.  The provided function must return an AnyPtr to
     // the 0th element, and each subsequent element must be sizeof(Element)
     // bytes after the next one, for a total number of elements equal to
     // whatever is read from or written to the accessor passed to length().
     //
     // If the length is 0, this may or may not be called.  You're allowed to
     // return null if the length is 0, but must not return null otherwise.
     //
     // The memory range must stay valid and consistent according to similar
     // rules to the AnyRef returned by computed_attrs().
     //
     // If contiguous_elems() is present, length() must also be present, and
     // elems() and computed_elems() must not be present.
    static constexpr
    DescriptorFor<T> auto contiguous_elems (
        Function<AnyPtr(T&)>*
    );

    ///// ACCESSORS
     // Accessors are internal types that are the output of the functions below.
     // They each have two associated types:
     //   - From type: The type of the item that the accessor is applied to
     //     (that's the type of the AYU_DESCRIBE block you're currently in).
     //   - To type: The type of the item that this accessor points to.
     // In addition, accessors can take these flags:
     //   - readonly: Make this accessor readonly (disable the AC::Writeable
     //     access capability).  If an accessor doesn't support writing, it is
     //     readonly by default and this flag is ignored.  If you have an attr
     //     or elem with a readonly accessor, the attr or elem should be flagged
     //     with invisible|optional or invisible|ignored, otherwise the parent
     //     item will not survive a round-trip serialize-deserialize.  This
     //     doesn't matter if the parent item is only ever going to be
     //     serialized and never deserialized.
     //   - unaddressable: Consider items accessed through this accessor to be
     //     unaddressable, even if they look like they should be addressable.
     //     There aren't many reasons to use this.
     //   - children_addressable: Normally you can only take the address of
     //     a child item if its parent is also addressable, but if the parent
     //     item's accessor has children_addressable, then instead you can
     //     take its address if the parent's parent is addressable (or if the
     //     parent's parent also has children_addressable, it's parent's
     //     parent's parent).  If you misuse this, you can potentially leave
     //     dangling pointers around, risking memory corruption, so be careful.
     //     This is intended to be used for reference-like proxy items, which
     //     are generated temporarily but refer to non-temporary items.
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
    static constexpr AcrFlags readonly = in::AcrFlags::Readonly;
    static constexpr AcrFlags children_addressable = in::AcrFlags::ChildrenAddressable;
    static constexpr AcrFlags unaddressable = in::AcrFlags::Unaddressable;
    static constexpr AcrFlags prefer_hex = in::AcrFlags::PreferHex;
    static constexpr AcrFlags prefer_compact = in::AcrFlags::PreferCompact;
    static constexpr AcrFlags prefer_expanded = in::AcrFlags::PreferExpanded;

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
    template <SameOrBase<T> T2, Describable M> static constexpr
    AccessorFromTo<T, M> auto member (
        M T2::* mp, AcrFlags = {}
    );

     // Give access to a const non-static data member.  This accessor will be
     // readonly, and is addressable and reverse_addressable.
    template <SameOrBase<T> T2, Describable M> static constexpr
    AccessorFromTo<T, M> auto const_member (
        const M T2::* mp, AcrFlags = {}
    );

     // Give access to a base class.  This can be any type where a C++ reference
     // to the derived class can be implicitly cast to a reference to the base
     // class, and a reference to the base class can be static_cast<>()ed to a
     // reference to the derived class.  This accessor is addressable and
     // reverse_addressable.
    template <Describable M> requires (
        requires (T* t, M* m) { m = t; t = static_cast<T*>(m); }
    ) static constexpr
    AccessorFromTo<T, M> auto base (AcrFlags = {});

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
    template <Describable M> static constexpr
    AccessorFromTo<T, M> auto ref_func (
        Function<M&(T&)>*, AcrFlags = {}
    );

     // Just like ref_func, but creates a readonly accessor.  Just like with
     // ref_func, be careful when returning a reference to a temporary.
    template <Describable M> static constexpr
    AccessorFromTo<T, M> auto const_ref_func (
        Function<const M&(const T&)>*, AcrFlags = {}
    );

     // This makes a read-write accessor based on two functions, one of which
     // returns a const ref to the child, and the other of which takes a const
     // ref to a child and writes a copy to the parent.  This accessor is not
     // addressable.  If possible, it's better to use member() than this.
    template <Describable M> static constexpr
    AccessorFromTo<T, M> auto const_ref_funcs (
        Function<const M&(const T&)>* get,
        Function<void(T&, const M&)>* set,
        AcrFlags = {}
    );

     // This makes a readonly accessor from a function that returns a child item
     // by value.  It is not addressable.
    template <Movable M> static constexpr
    AccessorFromTo<T, M> auto value_func (
        Function<M(const T&)>*, AcrFlags = {}
    );

     // This makes a read-write accessor from two functions that read and write
     // a child item by value.  It is not addressable.
    template <Movable M> static constexpr
    AccessorFromTo<T, M> auto value_funcs (
        Function<M(const T&)>* get,
        Function<void(T&, M)>* set,
        AcrFlags = {}
    );

     // This makes a read-write accessor from two functions, the first of which
     // returns a child item by value, the second of which takes a child item by
     // const reference and writes a copy to the parent.  This is usually what
     // you want if the child item is something like a std::vector that's
     // generated on the fly.
    template <Movable M> static constexpr
    AccessorFromTo<T, M> auto mixed_funcs (
        Function<M(const T&)>* get,
        Function<void(T&, const M&)>* set,
        AcrFlags = {}
    );

     // This makes an accessor to any child item such that the parent and child
     // types can be assigned to eachother with operator=.  It is not
     // addressable.
     //
     // I'm not sure how useful this is since you can just use value_funcs
     // instead, but here it is.
    template <Describable M> requires (
        requires (T t, M m) { t = m; m = t; }
    ) static constexpr
    AccessorFromTo<T, M> auto assignable (
        AcrFlags = {}
    );

     // This makes a readonly accessor which always returns a constant.  The
     // provided constant must be constexpr copy constructible.  This accessor
     // is not addressable, though theoretically it could be made to be.
    template <Copyable M> static constexpr
    AccessorFromTo<T, M> auto constant (
        const M& val, AcrFlags = {}
    );

     // Makes a readonly accessor which always returns a constant.  The pointed-
     // to constant does not need to be constexpr or even copy-constructible,
     // but it must be initialized before calling any AYU serialization
     // functions.  This accessor is addressable.
    template <Describable M> static constexpr
    AccessorFromTo<T, M> auto constant_ptr (
        const M* ptr, AcrFlags = {}
    );

     // Like constant(), but provides read-write access to a variable which is
     // embedded in the accessor with move().  This accessor is not constexpr,
     // so it cannot be used directly in an AYU_DESCRIBE block, and can only be
     // used inside computed_attrs, computed_elems, or anyref_func.  It is not
     // addressable.  There is no corresponding variable_pointer accessor
     // because if you're in an computed_attrs or computed_elems, you can just
     // convert the pointer directly to an ayu::AnyRef instead of using an
     // accessor.
     //
     // This is intended to be used for proxy types along with
     // children_addressable.
    template <Movable M> static // not constexpr
    AccessorFromTo<T, M> auto variable (
        M&& var, AcrFlags = {}
    );

     // An accessor that gives access to a child item by means of an ayu::AnyRef
     // instead of a C++ reference.  This and anyptr_func are the only accessors
     // whose child type can vary depending on the parent item it's applied to.
     //
     // Unlike computed_attrs and computed_elems, you should not return an empty
     // AnyRef from this function, or you may get null pointer derefs down
     // the line.
     //
     // This accessor is considered unaddressable, even if the returned anyref
     // is addressable.  Use anyptr_func to give access to an arbitrarily-typed
     // addressable item.
     //
     // The returned AnyRef must not be readonly unless this accessor is also
     // readonly.  Any prefer_* flags on the returned AnyRef will probably be
     // ignored.
     //
     // anyref_func and anyptr_func are not valid accessors for the keys and
     // length descriptors.
    static constexpr
    AccessorFrom<T> auto anyref_func (
        Function<AnyRef(T&)>*, AcrFlags = {}
    );
     // An accessor that gives access to a child item through an ayu::AnyPtr.
     // Like anyref_func but simpler.  Don't return empty or null from this.
     // The returned AnyPtr must not be readonly unless this accessor also has
     // the readonly flag.
    static constexpr
    AccessorFrom<T> auto anyptr_func (
        Function<AnyPtr(T&)>*, AcrFlags = {}
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
    template <
        Describable M,
        M&(T3::* get )()
    > static constexpr
    AccessorFromTo<T, M> auto ref_method (AcrFlags flags = {}) {
        return ref_func<M>(
            [](const T& v) -> M& { return (v.*get)(); }, flags
        );
    }
     // const_ref_method
    template <
        Describable M,
        const M&(T3::* get )()const
    > static constexpr
    AccessorFromTo<T, M> auto const_ref_method (AcrFlags flags = {}) {
        return const_ref_func<M>(
            [](const T& v) -> const M& { return (v.*get)(); }, flags
        );
    }
     // const_ref_methods
    template <
        Describable M,
        const M&(T3::* get )()const,
        void(T3::* set )(const M&)
    > static constexpr
    AccessorFromTo<T, M> auto const_ref_methods (AcrFlags flags = {}) {
        return const_ref_funcs<M>(
            [](const T& v) -> const M& { return (v.*get)(); },
            [](T& v, const M& m){ (v.*set)(m); },
            flags
        );
    }
     // value_method
    template <
        Describable M,
        M(T3::* get )()const
    > static constexpr
    AccessorFromTo<T, M> auto value_method (AcrFlags flags = {}) {
        return value_func<M>(
            [](const T& v) -> M { return (v.*get)(); }, flags
        );
    }
     // value_methods
    template <
        Describable M,
        M(T3::* get )()const,
        void(T3::* set )(M)
    > static constexpr
    AccessorFromTo<T, M> auto value_methods (AcrFlags flags = {}) {
        return value_funcs<M>(
            [](const T& v) -> M { return (v.*get)(); },
            [](T& v, M m){ (v.*set)(m); },
            flags
        );
    }
     // mixed_methods
    template <
        Describable M,
        M(T3::* get )()const,
        void(T3::* set )(const M&)
    > static constexpr
    AccessorFromTo<T, M> auto mixed_methods (AcrFlags flags = {}) {
        return mixed_funcs<M>(
            [](const T& v) -> M { return (v.*get)(); },
            [](T& v, const M& m){ (v.*set)(m); },
            flags
        );
    }

     // And here's an overload for init(), which is a descriptor, not an
     // accessor, but a lot of class-like types have a method for finishing
     // intialization, so this is useful.
    template <
        void(T3::* m )()
    > static constexpr
    DescriptorFor<T> auto init (double order = 0) {
        return init([](T& v){ (v.*m)(); }, order);
    }

    ///// INTERNAL

    template <DescriptorFor<T>... Dcrs> static constexpr
    auto AYU_describe (const Dcrs&... dcrs);
};

} // namespace ayu

#include "describe-base.inline.h"
