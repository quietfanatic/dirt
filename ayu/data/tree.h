 // This defines the main Tree datatype which represents an AYU structure.
 // Trees are immutable and reference-counted, so copying is cheap, but they
 // can't be accessed on multiple threads at a time.

#pragma once

#include <exception>
#include "../../uni/copy-ref.h"
#include "../common.internal.h"

namespace ayu {

 // For unambiguity, types of trees are called forms.
enum class Form : u8 {
    Undefined = 0,
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
     // A form that carries a stored exception, used for error reporting.
     // if you try to do anything with it, it will probably throw its contents.
    Error
};

 // Options that control how a Tree is printed.  These do not have any effect on
 // the semantics of the Tree, and they do not affect subtrees.
enum class TreeFlags : u8 {
     // For Number: Print the number as hexadecimal.
    PreferHex = 0x1,
     // For Array or Object: When pretty-printing, print this item compactly,
     // all on one line (unless one of its children is expanded).
     // For STRING: When printing in non-JSON mode, encode newlines and tabs as
     // \n and \t.
    PreferCompact = 0x2,
     // For Array or Object: When pretty-printing, print fully expanded with one
     // element/attribute per line.
     // For String: When printing in non-JSON mode, print newlines and tabs
     // as-is without escaping them.
     // If neither PREFER_EXPANDED nor PREFER_COMPACT is set, the printer will
     // use some heuristics to decide which way to print it.  If both are set,
     // which one takes priority is unspecified.
    PreferExpanded = 0x4,
     // For internal use only.  Ignore this.
    ValueIsPtr = 0x80,

    ValidBits = PreferHex | PreferCompact | PreferExpanded | ValueIsPtr
};
DECLARE_ENUM_BITWISE_OPERATORS(TreeFlags)

struct Tree {
    Form form = Form::Undefined;
     // Only the flags can be modified after construction.
    TreeFlags flags = {};

    constexpr bool has_value () const { return form != Form::Undefined; }

     // Default construction.  The only valid operation on an UNDEFINED tree is
     // has_value().
    constexpr Tree ();
     // Move construction.
    constexpr Tree (Tree&& o);
     // Copy construction.  May twiddle reference counts.
    constexpr Tree (const Tree&);
     // Destructor.
    constexpr ~Tree ();
     // Assignment.
    constexpr Tree& operator= (Tree&& o);
    constexpr Tree& operator= (const Tree& o);

    ///// CONVERSION TO TREE
    explicit constexpr Tree (Null, TreeFlags = {});
     // Disable implicit coercion of the argument to bool
    template <class T> requires (std::is_same_v<T, bool>)
    explicit constexpr Tree (T, TreeFlags = {});
     // Templatize this instead of providing an overload for each int type, to
     // shorten error messages about "no candidate found".
    template <class T> requires (
         // ACTUAL integer, not bool or char
        std::is_integral_v<T> &&
        !std::is_same_v<T, bool> && !std::is_same_v<T, char>
    )
    explicit constexpr Tree (T, TreeFlags = {});
     // May as well do this too
    template <class T> requires (std::is_floating_point_v<T>)
    explicit constexpr Tree (T, TreeFlags = {});

     // plain (not signed or unsigned) chars are represented as strings
     // This is not optimal but who serializes individual 8-bit code units
    explicit Tree (char v, TreeFlags f = {}) : Tree(UniqueString(1,v), f) { }
    explicit constexpr Tree (AnyString, TreeFlags = {});
     // Optimize raw char literals
    template <usize n>
    explicit constexpr Tree (const char(& v )[n], TreeFlags f = {}) :
        Tree(StaticString(v), f)
    { }

    explicit constexpr Tree (AnyArray<Tree>, TreeFlags = {});
    explicit constexpr Tree (AnyArray<TreePair>, TreeFlags = {});
    explicit Tree (std::exception_ptr, TreeFlags = {});

     // Convenient array/object construction because initializer_list sucks
    template <class... Args> static constexpr
    Tree array (Args&&... args) {
        return Tree(AnyArray<Tree>::make(std::forward<Args>(args)...));
    }

    template <class... Args> static constexpr
    Tree object (Args&&... args) {
        return Tree(AnyArray<TreePair>::make(std::forward<Args>(args)...));
    }

    ///// CONVERSION FROM TREE
     // These throw if the tree is not the right form or if
     // the requested type cannot store the value, e.g. try to convert to a
     // u8 a Tree containing the number 257.
    explicit constexpr operator Null () const;
    explicit constexpr operator bool () const;
    explicit constexpr operator char () const;
    explicit constexpr operator i8 () const;
    explicit constexpr operator u8 () const;
    explicit constexpr operator i16 () const;
    explicit constexpr operator u16 () const;
    explicit constexpr operator i32 () const;
    explicit constexpr operator u32 () const;
    explicit constexpr operator i64 () const;
    explicit constexpr operator u64 () const;
    explicit constexpr operator float () const { return double(*this); }
    explicit constexpr operator double () const;
     // Warning 1: The returned Str is not NUL-terminated.
     // Warning 2: The Str will be invalidated when this Tree is destructed.
    explicit constexpr operator Str () const;
     // This AnyString will remain valid though.  It shares the same refcounting
     // mechanism as Tree.
    explicit constexpr operator AnyString () const&;
    explicit constexpr operator AnyString () &&;
    explicit constexpr operator Slice<Tree> () const;
    explicit constexpr operator AnyArray<Tree> () const&;
    explicit constexpr operator AnyArray<Tree> () &&;
    explicit constexpr operator Slice<TreePair> () const;
    explicit constexpr operator AnyArray<TreePair> () const&;
    explicit constexpr operator AnyArray<TreePair> () &&;
    explicit operator std::exception_ptr () const;

    ///// CONVENIENCE
     // Returns null if the invocant is not an OBJECT or does not have an
     // attribute with the given key.
    constexpr const Tree* attr (Str key) const;
     // Returns null if the invocant is not an ARRAY or does not have an
     // element at the given index.
    constexpr const Tree* elem (u32 index) const;

     // Throws if the tree is not an object or doesn't have that attribute.
    constexpr const Tree& operator[] (Str key) const;
     // Throws if the tree is not an array or the index is out of bounds.
    constexpr const Tree& operator[] (u32 index) const;

    ///// INTERNAL

     // We could pack these into the same u32 as size, but we have extra room so
     // we may as well use it.
    bool floaty = false;
    bool owned = false;
    u32 size = 0;
    union {
        bool as_bool;
        i64 as_i64;
        double as_double;
        const char* as_char_ptr;
        const Tree* as_array_ptr;
        const TreePair* as_object_ptr;
        const std::exception_ptr* as_error_ptr;
    } data;
};

 // Test for equality.  Trees of different forms are considered unequal.
 //  - Unlike float and double, Tree(NAN) == Tree(NAN).
 //  - Like float and double, -0.0 == +0.0.
 //  - Objects are equal if they have all the same attributes, but the
 //    attributes don't have to be in the same order.
 //  - Undefined trees and trees with embedded errors always compare false, even
 //    to themselves.
bool operator == (const Tree& a, const Tree& b) noexcept;

 // Constrain to types that a Tree can be constructed from.  This is used in
 // value descriptors and attr_default.
template <class T>
concept ConstructsTree = requires (T v) { Tree(v); };

 // Tried to get something out of a tree that was the wrong form.
constexpr ErrorCode e_TreeWrongForm = "ayu::e_TreeWrongForm";
 // Tried to get something (probably a number) out of a tree but its value was
 // out of range for the requested type.  Example, u8(Tree(257)).
constexpr ErrorCode e_TreeCantRepresent = "ayu::e_TreeCantRepresent";

}  // namespace ayu

#include "tree.inline.h"
