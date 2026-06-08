AYU DATA LANGUAGE
=================

### Overview

- Null is `null`.
- Bools are `true` and `false`.
- Numbers can be decimal or hexadecimal.
    - You can write infinity with `1/0` and NaN with `0/0`.
- Strings use `"`.
    - Most single-word strings don't have to be quoted.
    - Nearly raw strings <code>\`</code> (backtick).
- Arrays use `[ ]` and contain elements separated by space and/or `,`.
- Objects use `{ }` and contain `key:value` attributes.
- Comments start with `--`.
- Macros are defined with `(name:value)` and used with `(name)`.

### Introduction

The AYU data language stores structured data in an easy-to-read format.

> _The name AYU does not stand for anything; it is just a cute-sounding name
> sitting in the highly saturated namespace of computer languages._

AYU is a near-compatible superset of JSON: most JSON is also valid AYU, and most
AYU can be mechanically translated into JSON.  Of all the known JSON descendants
it is most similar to Relaxed JSON (rjson).

### Definitions

This section defines some terms that are used throughout the documentation and
applies some restrictions to software working with AYU.  You can skip this
section if you just want to get into the meat and potatoes.

- A **document** is a span of text that stores machine-readable data.  It could
  be in a file or in memory or anywhere else.
- A **value** is an abstract piece of document data, which may contain other
  values.  A value has a **form**, not a type.  There are six forms a value can
  have: **null**, **bool**, **number**, **string**, **array**, **object**.
- An **item** is a concrete piece of program data, which may contain other
  items.  An item probably has a **type**.  Items can have a wide variety of
  types depending on the programming language and logical domain of the program.
  Your programming language might have its own terminology, calling them objects
  or values.  The AYU language imposes no constraints on what an item can be.

- A **parser** transforms a document into a value.
- A **printer** transforms a value into a document.
- A **deserializer** transforms a value into an item.
- A **serializer** transforms an item into a value.

- A **general-purpose parser** is one that purports to be able to parse all or
  nearly all AYU documents.  It supports all features in this specification, to
  the extent of applicable "must", "should", and "may" language, though it may
  freely issue warnings for any reason and may be configurable to constrain the
  documents it will accept.
- A **special-purpose parser** is one made for a specific program or domain.  It
  may constrain in any way the values and documents it is willing to parse.  It
  must reject documents it can't parse.  It must not change their meaning.

General-purpose and special-purpose printers can be defined accordingly, but a
general-purpose printer is much simpler than a general-purpose parser, because
it does not need to use macros, comments, or any other non-meaningful features.
In fact, any general-purpose JSON printer is very nearly a general-purpose AYU
printer.

Deserializers and serializers are always special-purpose.  They cannot be
general-purpose because there is no general-purpose system of items for them to
work with.  Therefore, a lone serializer or deserializer may freely reject or
transform values in any way they see fit.  The story is different if a
serializer and deserializer are affiliated and working in the same system of
items.  In this case, the serializer and deserializer should try to keep stable
as many properties of their items as they can when they are used together.  If
you serialize then deserialize an item, the deserialized item should behave the
same as the original in all meaningful ways.  If you reserialize the
deserialized item, the outputs of both serializations should be identical.

> _Some programs may prefer to operate directly on values instead of
> transforming them to and from items.  Such programs only need a parser and a
> printer.  They do not need a deserializer or serializer.  Alternatively, this
> could be thought of as making items identical to values, types identical to
> forms, and the deserializer and serializer the identity function._
>
> _There does not need to be a strict division between parsers/printers and
> de/serializers.  A program could parse and deserialize at the same time, or in
> several interleaved steps, or it could use some other paradigm that transcends
> the distinction in a way nobody has thought of before._

- If a property of a value is **preserved**, that means no matter how many times
  the value is printed and parsed back and forth, that property remains
  unchanged.  Preservation rules only apply to parsers and printers, not
  deserializers and serializers.
- If a property of a value or document is **non-meaningful**, that means the
  property must not have any observable effects on the behavior of any values
  and items derived from it.  Parsers and deserializers can still optimize their
  implementations differently based on the property, and serializers and
  printers can even change non-meaningful properties of values and documents
  they produce based on it, as long as all meaningful properties and logical
  behavior stay the same.

> _The non-meaningful principle means that if a program parses the number
> `0x1e`, for instance, it must take that as having the same meaning as `30`,
> because which format a number uses is non-meaningful.  However, if it later
> prints that number, it may choose to write it as `0x1e` instead of `30`
> because that was the format it read it in.  For another example, if a program
> parses, modifies, and prints a document, it may try to propagate comments and
> macros from the input document to the output document._

### Document specification

An AYU document is text in an ASCII-compatible format.

> _It's expected that AYU documents will be mainly UTF-8, but sometimes programs
> have to work with non-UTF-8 text.  If you need a strict UTF-8 string, you
> should validate it while deserializing it.  If you know your entire data
> domain is strictly UTF-8, you may validate the entire document before parsing
> it, as long as you also recognize runs of \xXX escapes and validate those
> too._

Printers must not put a Unicode Byte Order Mark at the front.  Parsers must skip
it or reject the document if it's there (check for the bytes `\xef\xbb\xbf`).

> _The BOM is forbidden in JSON.  We would love to follow their lead but some
> editors and tools automatically put one in UTF-8 files, causing confusing
> errors for inexperienced developers.  Parsers must detect a BOM to prevent it
> from ending up inside an unquoted string.  JSON does not have this problem._

A document contains one value of any form.  It is not restricted to an array or
an object.

> _JSON hasn't officially had this restriction since 2014._

Every value has one of these forms: null, bool, number, string, array, object.
Some forms have more than one format

#### Null

Indicated by the keyword `null`.

#### Bool

Populated by the keywords `true` and `false`.

#### Number

Numbers can be decimal, hexadecimal, or special.

Decimal numbers are composed of:

- an optional sign `+` or `-`,
- a run of decimal digits, containing an optional decimal point `.` anywhere
  except at the end,
- an optional exponent `e` or `E` followed by an optional sign, followed by a
  run of decimal digits which indicates a power of ten.

> _Numbers can start with a dot but they cannot end with one, because nobody
> writes numbers like that in real life and dots look too much like commas,_

Decimal numbers can have any number of leading zeroes, and they are still
interpreted as decimal, not octal.

Hexadecimal numbers are composed of:

- an optional sign `+` or `-`,
- the prefix `0x` or `0X`,
- a run of hexadecimal digits (higits, if you will).

The format of a number is non-meaningful.  `30`, `30.0`, `3e1`, and `0x1e` are
all the same number.

There are three special numbers with six names:

- Positive infinity is written as `1/0` or  `+1/0`.
- Negative infinity is written as `-1/0`.
- NaN (Not a Number) is written as `0/0`, `+0/0`, or `-0/0`.

These are matched exactly and no other variations are accepted.  `0/0.0` and
`2/0` are not allowed.

> _These names were chosen because they start the same way as ordinary numbers,
> so they don't add any restrictions to unquoted words._

What ranges and precisions of numbers are supported is implementation-defined.
A general-purpose parser should support at least IEEE 754 binary64 (double)
range and precision, and it must support at least binary32 (float) range and
precision along with all integers from -2^31 to 2^31-1.

> _The reference C++ implementation supports floating-point numbers of double
> precision and all integers from -2^63 to 2^63-1.

If a parser encounters a number that it can't support exactly, it should replace
it with the next lower or higher number that it can support.  A parser should
also replace a number outside of its supported range with positive or negative
infinity.  It should never replace a number with NaN.  A printer may print a
number that is not exactly equal to its input, as long as no input would result
in the same number being printed.  A parser and printer that are affiliated and
support the same numbers must preserve the distinctions between all numbers they
support, except for negative zero and negative NaN.  Negative zero only should
be preserved, and negative NaN only may be preserved.

> _Negative zero can have observable effects on floating point arithmetic.
> Negative NaN is usually difficult to detect.  The reference C++ implementation
> preserves negative zero but not negative NaN._
>
> _It might be wise for a parser to issue a warning when given a number that
> overflows its range or a number in pure integer format that it can't store
> exactly.  A special-purpose parser that knows its target domain only supports
> integers would do better to proactively reject numbers with decimal points and
> exponents than to silently round them to integers._

#### String

Strings can be quoted, unquoted, or nearly raw.  Which format a string has is
non-meaningful.

##### Quoted strings

Double quotes `"` delimit a quoted string.  Quoted strings may span multiple
lines and have any bytes except for unescaped `"` and `\`.  They may have the
following escape sequences:

- `\n` is a Newline (LF), equivalent to `\x0a`.
- `\r` is a Carriage Return (CR), equivalent to `\x0d`.
- `\t` is a Tab, equivalent to `\x09`.
- `\"` is a literal quote.
- `\\` is a literal backslash.
- `\xXX` is a byte (not a codepoint) with a two-digit hexadecimal value, which
  may be part of a multibyte character.
- `\u{XXX...}` is a unicode codepoint with anywhere from 1 to 6 hexadecimal
  digits.  The codepoint must be U+10FFFF or less, and will be encoded as one to
  four UTF-8 bytes.  This does not recognize UTF-16 surrogates; it will just
  encode them the same way as any other codepoint.

Also these escape sequences are supported for compatibility with JSON.  Their
use is not recommended.

- `\b` is a Backspace, equivalent to `\x08`.
- `\f` is a Form Feed, equivalent to `\x0b`.
- `\/` is a literal slash, equivalent to just `/`.
- `\uXXXX` is a UTF-16 code unit, which may be part of a surrogate pair.  If an
  escape for a high surrogate (`\uD800` to `\uDBFF`) is immediately followed
  by an escape for a low surrogate (`\uDCOO` to `\uDFFF`), they will be joined
  into one codepoint and encoded as a four-byte UTF-8 sequence.  Otherwise, one
  `\uXXXX` escape will be encoded as a one-to-three byte sequence.  An
  unmatched surrogates is treated like normal characters and encoded as a
  three-byte sequence.  Escaped surrogates do not pair with unescaped
  surrogates.

A backslash followed by anything else, including a line ending, is an error.

Whether a character was written in escaped or unescaped format is non-meaningful.

It is recommended but not required for all unprintable characters to be escaped
and for all strings to be valid UTF-8 after decoding all escapes.

> _Non-UTF-8 strings are incompatible with conformant JSON, but we support them
> because POSIX filenames and other sources of text can have arbitrary bytes
> that aren't valid UTF-8._
>
> _AYU documents are not required to be UTF-8, but only an ASCII-compatible
> encoding.  Regardless, `\u` escapes are specced to always output UTF-8.  If
> an AYU document uses a non-UTF-8 encoding, it should avoid `\u` escapes lest
> it suffer the curse of mojibake._

##### Unquoted strings

An unquoted string is also called a word.  These are allowed inside a word:

- Word characters
- Double-colon `::` if it has a word character on each side

These are considered word characters:

- Letters and digits
- Most symbols
    - `!` `#` `$` `%` `&` `'` `\*` `+` `-` `.` `/` `<` `=` `>` `?` `@` `^` `\_`
      `|` `~`
- Any non-ASCII bytes (>= `\x80`)

Conversely, these are not word characters:

- ASCII whitespace and control codes (`\x7f` and anything `\x1f` or less)
- Things that look like delimiters, separators, quotes, or escapes:
    - `[` `]` `{` `}` `(` `)` `,` `;` `:` `"` <code>\`</code> `\`

> _To avoid forcing parsers to decode UTF-8 and import megabytes of Unicode
> tables, all non-ASCII Unicode is allowed in unquoted strings.  Please do not
> abuse this privilege.  If it looks like whitespace or syntax, quote it.  A
> Unicode-aware parser might warn about weird or deceptive characters in
> unquoted strings.

Words cannot start in a way that makes them look like numbers or comments.
Specifically, they cannot start with any of:

- A digit
- `.` then a digit
- `+` then `.` or a digit
- `-` then `-`, `.`, or a digit

> _You only have to look at two characters to determine if a value is a number._

Finally, the whole word cannot be any of the keywords `null` `true` `false`.

> _The unquoted string rules were chosen to include most relative IRI
> references.  Full IRIs with a scheme still have to be quoted because they
> contain a colon.  Some characters that are not used for syntax are reserved
> for future extensions or simply because they look like they could be syntax.
> Double colon is here as a special affordance for type names in C++ and related
> languages.  There is a chance this affordance will disappear before this spec
> becomes too public to change.
>
> _Unquoted strings do not allow any form of escaping.  They are the same on
> both the inside and the outside._

Oh, there's one more pesky rule that is technically necessary, but hopefully is
never relevant.  If an unquoted word is at the very start of the document (byte
offset 0), it cannot start with the bytes `\xef\xbb\xbf`, which could be
interpreted as a Byte Order Mark.

> _This rule is a necessary consequence of allowing Unicode in unquoted strings.
> It would be annoying for a parser to have to check for a BOM in every word,
> because lots of ordinary Unicode word codepoints start with `\xef`, so instead
> they're only required to check at the beginning of the document.  If a
> Unicode-unaware printer quotes all strings with non-ASCII bytes, it does not
> have to worry about this.  A Unicode-aware printer should just add U+FEFF to
> its set of always-escaped codepoints._
>
> _A strict reading of this rule implies that if a document starts with two
> BOMs, the second one is the start of an unquoted string.  Pray this never
> happens._

##### Nearly raw strings

A nearly raw string is delimited by backticks <code>\`</code>.  The only escape
it allows is a double backtick, which is replaced with a single backtick.
Backslashes have no special meaning and are passed on as-is.

```
`a\`b` -- ERROR: b is past the end of the string.
`a\``b` -- Equivalent to "a\\`b"
`Use "\n" for a newline` -- Equivalent to "Use \"\\n\" for a newline"
```

> _For familiarity and readability, it's recommended to use `"` most of the
> time, and only use <code>\`</code> for regexes, Windows filepaths, and other
> things susceptible to Leaning Toothpick Syndrome._

#### Array

Arrays are delimited by square brackets `[` and `]` and can contain multiple
values, called elements.

Commas are allowed but not required between elements, and a final comma is
allowed.  Multiple commas in a row are not allowed, nor is an initial comma.
Keywords, numbers, and strings (even if quoted) must have whitespace or a comma
between one another.  Arrays, objects, and macro definitions and invocations can
be close and comfy with anything.

#### Object

Objects are delimited by curly braces `{` and `}` and contain key-value pairs,
called attributes.  An attribute is a string (the key), followed by a colon `:`,
followed by a value.  Comma and whitespace rules are the same as for arrays.

Keys are parsed exactly the same way as strings, including whether they can be
unquoted or not.  Keys that look like keywords or numbers must be quoted.

Parsers should reject objects with duplicate keys unless they need strict JSON
compatibility.  The order of attributes in an object should be non-meaningful,
but should be preserved for readability.

> _If you think you want an object with order-significant attributes or multiple
> attributes with the same key, use an array of pair-arrays instead._

#### Comments

A comment starts with `--` and continues to the next newline byte or the end of
the document.  It can appear anywhere whitespace is allowed except right on the
end of a keyword, number, or string, because `-` is a word character.

Comments are non-meaningful and should not contain type annotations, parsing
directives, or anything of that sort.

#### Macros

Macros let you name values to reuse later on.

A macro definition is `(`, then a string (the name), then `:`, then a value,
then `)`.  It can only occur before a value or key or another macro definition.
The definition is effective starting with the `)` and lasting until the
construct that contains it ends.  That construct can be an array, an object, or
the entire document.  A macro definition cannot contain another macro
definition.

A macro invocation is `(`, then a string (the name), then `)`.  A macro
invocation can occur anywhere a value or key can occur.  If the name matches the
name of a currently-defined macro, then the invocation will be replaced with
that definition's value, otherwise it is an error.

A macro invocation can be used as a key in an object if its value is a string.
It cannot be used as the name of another macro.

Macros are replaced eagerly, cannot be recursive, and cannot be redefined in the
same scope.  Whether they can be shadowed by a definition in an inner scope is
implementation-defined.

```
(foo: 1)
(bar: [(foo) (foo)]) -- (bar) is now [1 1]
(bar: [(foo) (foo)]) -- ERROR: conflicting definition
[1 (a:2) 3 (a)] -- [1 3 2]
{(k:woof) (k):(k)} -- {woof:woof}
{(k:bark) (k):(k)} -- {bark:bark} -- macros are scoped
(k) -- ERROR: (k) isn't defined any more
```

The default behavior when encountering an undefined macro should be to reject
the document and emit an error.  A parser may have an API to provide predefined
macros before parsing a document, but if it's a general-purpose parser it must
not predefine any macros by default.  It may also have an API to parse a
document with undefined macros and fill them in later, in the vein of an SQL
prepared statement.

Macro names are parsed the same as strings, including rules for when they have
to be quoted, except that a macro name cannot have a macro definition or
invocation in it.

> _The macro syntax is designed to have as little effect on the rest of the
> grammar as possible, yet have potential to expand into a more complex macro
> system in the future.  Macros are scoped to preserve the property that you can
> insert one document into another without affecting the outer document (don't
> actually do this with string concatenation though).

If there is a maximum nesting depth, it should apply to the document both before
and after replacing all macros.

```
 -- Suppose maximum nesting depth is 3
[[ (foo: [[]] ) ]] -- ERROR: too deep before replacing macros
[[ (foo) ]] -- ERROR: too deep after replacing macros
```

Macros are transparent, in the sense that they must not be visible to ordinary
program logic.  Whether a macro was used for a particular value or not is
non-meaningful.

#### Whitespace

The only characters considered whitespace for syntactic purposes are space,
newline, carriage return, and tab.  Exotic ASCII whitespaces like form feed and
vertical tab are forbidden outside of quoted strings and comments.  Unicode
whitespace characters are considered word characters, but a Unicode-aware parser
might warn if they are taken that way.

#### Limits

Parsers and printers may limit the length of documents they are able to process,
and this limit may depend on the capabilities of the hardware they're running
on.  A general-purpose parser should accept documents at least one billion bytes
long if its hardware is capable of it.

Parsers and printers should enforce a maximum nesting depth for arrays and
objects, which may be configurable.  If a general-purpose parser has a depth
limit, its default limit must be at least 50.

General-purpose parsers and printers should support strings of at least one
billion bytes, arrays of at least 128 million elements, and objects of at least
64 million attributes if their hardware is so capable.

#### Compatibility with JSON

##### Special numbers

JSON does not have infinities or NaN.  When translating AYU to JSON, they should
be changed to the case-sensitive quoted strings `"Infinity"`, `"-Infinity"`, and
`"NaN"`.  Deserializers should accept these strings as values for floating point
types.  There is minor potential for data loss if a deserializer can accept
either numbers or strings for an item, and these names could have been
acceptable values for that item with different meanings than the special numbers.
Hopefully this should be rare in practice.

> _These names seem to be the most common choices for representing the special
> numbers in JSON, because JavaScript recognizes them in its implicit coercions.
> Sometimes `null` is used for NaN, but this is not recommended because many
> JSON serializers render both NaN and infinities as `null`._

##### Non-UTF-8 strings

JSON documents and their strings are required to be UTF-8, and many JSON parsers
will reject documents with invalid UTF-8.  Because of this, when translating AYU
to JSON, something must be done with strings that don't contain valid UTF-8.
There are a few options, none of which work for all scenarios.

1. Refuse to translate the document and throw an error.  In theory this prevents
data loss because it should prompt a human operator to find and deal with the
problem at the source.  But in practice, it can cause loss of the entire
document if a machine silently drops it or the operator gives up on fixing it.
2. Censor the strings by replacing invalid bytes or sequences with U+FFFD (the
Replacement Character).  This is the standard and most commonly recommended
approach for sanitizing UTF-8.  However, this loses the data that was in the
invalid bytes, leading to potential consequences like filenames that were
different becoming identical and causing the files to overwrite one another.
3. Reinterpret the invalid bytes as Latin-1.  This can cause minor data loss
because the reinterpreted bytes may conflict with legitimate characters between
U+80 and U+FF, but the likelyhood of conflict between full strings is low.
Hopefully a human operator will notice the mojibake and, if they care enough,
recover the original data.
4. Embed the invalid bytes into another character range that is unlikely or
invalid in Unicode text.  This in theory can cause less data loss than using the
U+80 to U+FF range, but it requires a parser on the other end to agree on the
same embedding scheme, and is unlikely to be understood by a human operator.
Every choice of where to embed them also has its own ramifications.
5. Replace the string with an array of numbers.  This might preserve the most
data but it only works with serializers that understand what it means, and it
removes the human-readability of non-corrupted parts of the string.
6. Pass the invalid bytes through unaltered.  This produces nonconformant JSON,
but it's the easiest option if you know that any programs that will read it
don't care.  If they do care, this pushes the burden of choice onto them.

If you can't decide which option to pick, pick option 3.

##### Object attributes

The JSON spec does not mandate that objects have unique keys.  In practice,
there are almost no JSON documents that have duplicate keys in objects, and many
general-purpose JSON parsers will reject such objects or misbehave on them
(particularly ones that store attributes in a hash map).  An AYU parser may
accept duplicate keys if it needs 100% compatibility with JSON, but it's
recommended not to.

Similarly, the JSON spec does not mandate that the order of attributes in
objects is non-meaningful.  Similarly again, there are almost no JSON documents
where the order is meaningful, and many JSON parsers don't even expose the order
of attributes in their API (again, like ones that use hash maps).

### Tips for serializing and deserializing

For the most part, AYU does not constrain the behavior (or existence) of
serializers and deserializers.  Therefor this section is non-normative, and only
constitutes advice, not specifications.

When transforming a value to an item or an item to a value, the mapping between
forms and types can be diverse and complex.

The same form can represent different types.  For instance a string could
represent a chunk of text, or it could select a value of an enum, or it could
name a type.  An array could represent a list of items, or it could specify
which bits are set in a bitfield.  An object could be a struct with a fixed set
of attributes, or it could be a hash map with arbitrary strings for keys.

On the other side, some types can be represented by multiple forms.  For
instance, a matrix-like type might become an array of numbers, or it might
become a string denoting a special matrix, like `id` or `flipx`.  A common
structure could be written as either an array or an object to choose between
brevity and clarity.

Here are some recommendations on how to serialize some non-straightforward item
types that don't seem to fit neatly into value forms.

#### Dynamically-typed items

Like JSON, AYU does not have type annotations.  The types of items are usually
implicit, with the interpretation of a value depending on the interpretation of
its parent.  Where types are explicit, those types must themselves be
represented as values.

The recommended way to serialize an item that could have multiple types is to
use an array of two elements, the first of which is a type name (in string form)
and the second of which is the item's content.

```
[float 3.5]
[app::Settings {foo:3 bar:4}]
[std::vector<int32> [408 502]]
```

#### Optional types

Types that may or may not contain an item, such as C++'s `std::optional` or
`std::unique_ptr`, can be serialized as an array of one or zero elements.  If
they are the value of an object's attribute, they can alternatively be
represented by the presence or absence of the entire attribute.  If you know
that the contained item can never be `null`, you can also use `null` to indicate
a lack of content.

#### Binary data

AYU does not have a blob form.  To represent binary data, you can use an array
of integers, or a string of hexadecimal digits, or a filepath pointing to a
separate binary file.  You can also use a non-UTF-8 string, but that's
incompatible with JSON.

#### Pointers, references, links, etc

The AYU serialization library represents links and pointers in the form of IRI
strings.  As an example,

```
[ayu::Collection {
    some_object: [MyObject {
        foo: 50
        bar: [60 70 80 90]
    }]
     -- The following makes some_pointer point to some_object.bar[2].
     -- # -> The current document's value (skipping its type)
     -- #/some_object -> [MyObject {foo:50 bar:[60 70 80 90]}]
     -- #/some_object+1 -> {foo:50 bar:[60 70 80 90]}
     -- #/some_object+1/bar -> [60 70 80 90]
     -- #/some_object+1/bar+2 -> 80
    some_pointer: [int32* #/some_object+1/bar+2]
     -- The following points to an item in another file.
    another_pointer: [AnotherObject* /folder/file.ayu#/target+1]
]]
```

The process of converting a pointer to an IRI is quite heavyweight, as it might
require scanning large amounts of data to find the pointer's target.  A simpler
serializer could require any item that can be pointed to to have an ID or name,
and use that ID or name to serialize pointers.

### Potential future features

These are features that may or may not be added to the language in a future
version.  If they are added, they will probably take the given syntax, and other
future features are unlikely to conflict with the syntax.

- Rational numbers like `34/56`, expanding the special number syntax.
- Explicit indexes in arrays with `[0:x 1:y]`.
- Block comments like `(-- comment --)`.
- Allowing numeric macro names, to facilitate "prepared statement"-like APIs.
- Arithmetic operations that distribute over arrays, so that `(16 * [[1 1] [2 2]
  [3 3]])` yields `[[16 16] [32 32] [48 48]]`.  This would only take place
  inside `( )` so it would not conflict with anything else.
- Including other files as document fragments, strings, or arrays of numbers
  with `(include asdf.ayu)`, `(file asdf.txt)`, and `(embed u32le asdf.bin)`.
- And we may as well implement the above two features as predefined macros in a
  pattern-matching macro system.  It would also allow something like the below
  example.

```
([(a) (b)] @+ [(c) (d)]: [((a) + (c)) ((b) + (d))])
([1 2] @+ [3 4]) -- yields [4 6]`
```

### Rejected features

These are features that were considered for AYU at one point, but ultimately
rejected for one reason or another.

- Octal and binary numbers.  Octal is rarely used and binary isn't important
  enough when hexadecimal is present.
- Hexadecimal floating point numbers.  An earlier version of this language
  included these, but they were deemed too rare to be worth the implementation
  burden in environments that don't have a bulitin parser for them.  Modern
  floating point parsers and printers can preserve precision perfectly in
  decimal format.
- Single-quoted strings.  If they work the same as double-quoted strings, they
  don't help enough to be worth it.  If they work as nearly raw strings, then
  backtick is better because backticks are much rarer.  If they do something yet
  different, then we'll have four different string formats, and three is already
  too many.  Furthermore, every language with single-quoted strings treats them
  differently, so any choice we make will be surprising to someone.  Sorry,
  single-quote lovers.  YAML, JSON5, and Relaxed JSON are still there for you.
- Indentation-controlled strings.  These are quite complicated to implement.
  Putting a long multi-line string in the middle of a data structure also breaks
  up the structure making it harder to read.  It's better to put the string in a
  macro or even another file.
- Using `:` or `;` in arrays to reduce square bracket usage, allowing `[[a b] [c
  d]]` to be written as `[a:b c:d]` or `[a b; c d]`.  These would not be obvious
  to someone who only knows JSON and hasn't read the spec.  They'd also be
  error-prone, in that missing a `:` or `;` would silently change the data to
  something valid.
- Non-string keys in objects.  This would be very easy to parse, but not many
  deserializers would make use of it.  It wouldn't be very valueable, given you
  can just an array of pairs instead.  And we haven't even mentioned JSON
  compatibility.
- Attributes with no value in objects, like `{a:b c d:e}` to mean `{a:b c:null
  d:e}`.  This was never seriously considered, because it only saves five
  characters and is highly error-prone.
- Comments using `//` or `#` for similarity with other JSON-likes.  These would
  add more restrictions to unquoted strings that you'd have to remember.  I
  considered using `;` like in .ini-style languages, but `;` looks too similar
  to `:` (it's fine for them because they use `=` for key-value pairs).  `--` is
  also more visually distinct than other alternatives.  It makes comments look
  like negative space, constrasting with the positive space of data.
- Macros using `$var` syntax.  I really wanted this but it added an extra
  restriction to unquoted strings, and it was unclear how to make macro
  definitions look good without causing even more restrictions or being weird to
  parse.
