AYU DATA LANGUAGE
=================

## 0. Overview

- Null is `null`.
- Bools are `true` and `false`.
- Numbers can be decimal or hexadecimal.
    - You can write infinity with `1/0` and NaN with `0/0`.
- Strings use `"`.
    - Most single-word strings don't have to be quoted.
    - Nearly raw strings use <code>\`</code> (backtick).
- Arrays use `[ ]`, commas optional.
- Objects use `{ }` with `key:value` attributes, commas optional.
- Comments start with `--`.
- Macros are defined with `(name:value)` and used with `(name)`.

## 1. Introduction

AYU is a data language to store structured data in an easy-to-read format.

The design priorities of AYU are:

1. **Ease of reading and writing by humans**
    - Familiar without having to read the spec
    - Low memorization burden
    - Avoids surprises, gotchas, and easy mistakes
2. **Ease of reading and writing by computers**
    - Reasonably fast to parse and print
    - Relatively straightforward to implement
3. **Semantic simplicity**
    - Type agnostic
    - Finite set of value forms
4. **Semantic universality**
    - Able to represent almost any kind of data
5. **Semantic consistency** (AKA why the spec is so long)
    - Tries to guarantee stability of data
    - Minimal ambiguities and corner cases

Pursuing these priorities resulted in a medium-complexity data language that is
nearly compatible with JSON.  AYU is not as simple as most JSON-likes, nor as
complex as YAML.  It is more generous than JSON5 and less generous than Relaxed
JSON.  It has unintrusive macros.

> _The name AYU does not stand for anything.  It is just a cute-sounding name
> sitting in the highly saturated namespace of computer languages._

## 2. Definitions

This section defines some terms that are used throughout the specification and
applies some restrictions to software working with AYU.  This section has a lot
of legalistic details, so you can skip it if you just want to get into the meat
and potatoes.

> _Any text that is presented like this is a non-normative comment._

The words **must**, **should**, and **may** have the usual RFC 2119 definitions.

A **document** is some text that stores data according to the AYU specification.
It could be in a file or in memory or anywhere else.

A **value** is an abstract piece of document data, which may contain other
values.  A value has a **form**, not a type.  The **scalar** forms are **null**,
**bool**, **number**, and **string**.  The **compound** forms are **array** and
**object**.  Some forms, namely number and string, have multiple **formats**
that their values can be written in.  Which form a value has is a core part of
its identity.  Which format it is written in is not.

An **item** is a concrete piece of program data, which may contain other items.
An item probably has a **type**.  Items can have a wide variety of types
depending on the programming language and logical domain of the program.  Your
programming language might have its own term for items, calling them objects or
values.  AYU imposes no constraints on what items can be, or even that they
exist at all.

Typically, one value **represents** one item, but not always.

A **parser** transforms a document into a value.

A **deserializer** transforms a value into an item.

A **serializer** transforms an item into a value.

A **printer** transforms a value into a document.

> _By way of illustration, a parser would take the document `[4.1 7.5]` and
> transform it into an array value containing two number values.  A deserializer
> would then transform it into a complex number, a 2-dimensional vector, an
> interval, or anything else that can be thought of as a pair of numbers.  A
> serializer would transform it back into an array value, and a printer would
> transform it back into a document._
>
> _The four functions of parser, deserializer, serializer, and printer, are
> conceptual functions.  Their role is to help understand how AYU is processed,
> not to constrain how software is organized.  Each function could correspond to
> a separate program component, but it doesn't have to.  A program could parse
> and deserialize at the same time, or in interleaved chunks, or with visitor
> callbacks, or it could use some other paradigm that transcends the distinction
> in a way nobody has thought of before._
>
> _Some programs may prefer to operate directly on values instead of
> transforming them to and from items.  Such programs only need a parser and a
> printer.  They do not need a deserializer or serializer.  Alternatively, this
> could be thought of as making items identical to values, types identical to
> forms, and the deserializer and serializer the identity function._
>
> _Serializers and deserializers are not as specific to AYU as parsers and
> printers are.  A serializer or deserializer made for another JSON-like
> language could easily be adapted to work with AYU, and vice versa._

A **general-purpose parser** is one that purports to be able to parse all or
nearly all AYU documents.  It supports all features in this specification, to
the extent of applicable "must", "should", and "may" language, though it may
freely issue warnings for any reason and may be configurable to constrain the
documents it will accept.

A **special-purpose parser** is one made for a specific program or domain.  It
may constrain in any way the values and documents it is willing to parse.  It
must reject documents it can't parse.  It must not change their meaning.

General-purpose and special-purpose printers can be defined accordingly, but a
general-purpose printer is much simpler than a general-purpose parser, because
it does not need to use macros, comments, or other non-meaningful features.  In
fact, any general-purpose JSON printer is very nearly a general-purpose AYU
printer.

Deserializers and serializers are always special-purpose.  They cannot be
general-purpose because there is no such thing as a general-purpose system of
items and types for them to work with.  Therefore, a lone serializer or
deserializer may freely reject or transform values in any way it sees fit.  The
story is different if a serializer and deserializer are affiliated and working
with the same type system.  In this case, the serializer and deserializer should
try to keep stable as many properties of their items as they can when they are
used together.  If you serialize then deserialize an item, the deserialized item
should behave the same as the original in all meaningful ways.  If you
reserialize the deserialized item, the outputs of both serializations should be
identical.

If a property of a value is **preserved**, that means no matter how many times
that value is printed and parsed back and forth, the property remains
unchanged.  Preservation rules only apply to parsers and printers, not to
deserializers and serializers.

If a property of a value or document is **non-meaningful**, that means the
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

## 3. Document Syntax

An AYU document is text in an ASCII-compatible encoding.

> _It's expected that AYU documents will be mainly UTF-8, but sometimes programs
> have to work with non-UTF-8 text.  If you need a strict UTF-8 string, you
> should sanitize it while deserializing it.  If you know your entire data
> domain is strictly UTF-8, you may sanitize the entire document before parsing
> it, as long as you also recognize runs of \xXX escapes and sanitize those
> too._

Printers must not put a Unicode Byte Order Mark at the front.  Parsers must skip
it or reject the document if it's there (check for the byte sequence 0xEF 0xBB
0xBF).

> _The BOM is forbidden in JSON, but some editors and tools automatically put it
> in UTF-8 files, causing confusing errors for inexperienced developers.  AYU
> parsers must detect a BOM to prevent it from ending up inside an unquoted
> string, a problem JSON does not have._

A document contains one value of any form.  It is not restricted to an array or
an object.

> _JSON hasn't officially had this restriction since 2014._

Every value has one of these forms: null, bool, number, string, array, object.

### 3.1. Null

Indicated by the keyword `null`.

### 3.2. Bool

Populated by the keywords `true` and `false`.

### 3.3. Number

Numbers can be decimal, hexadecimal, or special.

#### 3.3.1. Decimal Numbers

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

#### 3.3.2. Hexadecimal Numbers

Hexadecimal numbers are composed of:

- an optional sign `+` or `-`,
- the prefix `0x` or `0X`,
- a run of hexadecimal digits.

The format of a number is non-meaningful.  `30`, `30.0`, `3e1`, and `0x1e` are
all the same number.

#### 3.3.3. Special Numbers

There are three special numbers with six names:

- Positive infinity is written as `1/0` or  `+1/0`.
- Negative infinity is written as `-1/0`.
- NaN (Not a Number) is written as `0/0`, `+0/0`, or `-0/0`.

These are matched exactly and no other variations are accepted.  `0/0.0` and
`2/0` are not allowed.

> _These names were chosen because they start the same way as ordinary numbers,
> so they don't add any restrictions to unquoted words._

#### 3.3.4. Range and Precision of Numbers

What ranges and precisions of numbers are supported is implementation-defined.
A general-purpose parser must support at least:

- IEEE 754 binary32 (float) range and precision, not including subnormals
- All integers from -2^31 to 2^31-1

And it should support at least:

- IEEE 754 binary64 (double) range and precision, including subnormals
- All integers from -2^63 to 2^63-1

If a parser encounters a number that it can't support exactly, it should replace
it with the next lower or higher number that it can support.  A parser should
also replace a number outside of its supported range with positive or negative
infinity.  It should never replace a number with NaN.  A printer may print a
number that is not exactly equal to its input, as long as no other input would
result in the same number being printed.  A parser and printer that are
affiliated and support the same numbers must preserve the distinctions between
all numbers they support, except possibly for negative zero and negative NaN.
Negative zero should be preserved, and negative NaN may be preserved.

> _Negative zero can have observable effects on floating point arithmetic.
> Negative NaN is comparatively difficult to detect.
>
> _AYU numbers have no way to distinguish signalling NaNs or NaNs with payloads.
> A serializer that needs to preserve those will have to use a custom encoding
> with strings or some other form._
>
> _It might be wise for a parser to issue a warning when given a number that
> overflows its range or a number written in pure integer format that it can't
> store exactly.  A special-purpose parser for a domain that only supports
> integers would do better to proactively reject non-integers than to silently
> round them to integers._
>
> _Although this spec recommends supporting 64-bit integers, many programming
> languages only use double-precision floating point for their numbers.  Be
> aware that integers beyond ±2^53 may not be preserved when interacting with
> those languages._

### 3.4. String

Strings can be quoted, unquoted, or nearly raw.  Which format a string has is
non-meaningful.

#### 3.4.1. Quoted Strings

Double quotes `"` delimit a quoted string.  Quoted strings may span multiple
lines and have any bytes except for unescaped `"` and `\`.  They may have the
following escape sequences:

- `\n` is a newline (LF), equivalent to `\x0a`.
- `\r` is a carriage return (CR), equivalent to `\x0d`.
- `\t` is a tab, equivalent to `\x09`.
- `\"` is a literal quote.
- `\\` is a literal backslash.
- `\xHH` is a byte (not a codepoint) with a two-digit hexadecimal value, which
  may be part of a multibyte character.
- `\u{HHH...}` is a Unicode codepoint with anywhere from 1 to 6 hexadecimal
  digits.  The codepoint must be U+10FFFF or less and will be encoded in one to
  four UTF-8 bytes.  This does not recognize UTF-16 surrogates; it will just
  encode them the same way as any other codepoint.

Also these escape sequences are supported for compatibility with JSON.  Their
use is not recommended.

- `\b` is a backspace, equivalent to `\x08`.
- `\f` is a form feed, equivalent to `\x0b`.
- `\/` is a literal slash, equivalent to just `/`.
- `\uHHHH` is a UTF-16 code unit, which may be part of a surrogate pair.  If an
  escape for a high surrogate (`\uD800` to `\uDBFF`) is immediately followed by
  an escape for a low surrogate (`\uDCOO` to `\uDFFF`), they will be joined into
  one codepoint and encoded as a four-byte UTF-8 sequence.  Otherwise, one
  `\uHHHH` escape will be encoded as a one-to-three byte sequence.  An unmatched
  surrogate is treated like a normal character and encoded as a three-byte
  sequence.  Escaped surrogates do not pair with unescaped surrogates.

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

#### 3.4.2. Unquoted Strings

An unquoted string is also called a word.  These are allowed inside a word:

- Word characters
- Double-colon `::` if it has a word character on each side

These are considered word characters:

- Letters and digits
- Most symbols:
    - `!` `#` `$` `%` `&` `'` `*` `+` `-` `.` `/` `<` `=` `>` `?` `@` `^` `_`
      `|` `~`
- Any non-ASCII bytes (0x80 or higher)

Conversely, these are not word characters:

- ASCII whitespace and control codes (0x7F and anything 0x1F or less)
- Things that look like delimiters, separators, quotes, or escapes:
    - `[` `]` `{` `}` `(` `)` `,` `;` `:` `"` <code>\`</code> `\`

> _To avoid forcing parsers to decode UTF-8 and import megabytes of Unicode
> tables, all non-ASCII Unicode is allowed in unquoted strings.  Please do not
> abuse this privilege.  If it looks like whitespace or syntax, quote it.  A
> Unicode-aware parser might warn about weird or deceptive characters in
> unquoted strings._

The first two characters of a word are restricted so it can't look like a number
or a comment.  Specifically, it cannot start with any of:

- A digit
- `.` then a digit
- `+` then `.` or a digit
- `-` then `-`, `.`, or a digit

Finally, the whole word cannot be any of the keywords `null` `true` `false`.

> _The unquoted string rules were chosen to include most relative IRI
> references.  Full IRIs with a scheme still have to be quoted because they
> contain a colon.  Some characters that are not used for syntax are reserved
> for future extensions or simply because they look like they could be syntax.
> Double colon is here as a special affordance for type names in C++ and related
> languages._
>
> _Unquoted strings do not allow any form of escaping.  They are the same on
> both the inside and the outside._

Oh, there's one more pesky rule that hopefully is never relevant.  If an
unquoted word is at the very start of the document (byte offset 0), it cannot
start with the byte sequence 0xEF 0xBB 0xBF, which could be interpreted as a
Byte Order Mark.

> _This rule is a necessary consequence of allowing Unicode in unquoted strings.
> It would be annoying for a parser to have to check for a BOM in every word,
> because lots of ordinary codepoints start with 0xEF, so instead it's only
> required to check at the beginning of the document.  If a Unicode-unaware
> printer quotes all strings with non-ASCII bytes, it does not have to worry
> about this.  A Unicode-aware printer should just add U+FEFF to its set of
> always-escaped codepoints._
>
> _A strict reading of this rule implies that if a document starts with two
> BOMs, the second one is the start of an unquoted string.  Pray this never
> happens._

#### 3.4.3. Nearly Raw Strings

A nearly raw string is delimited by backticks <code>\`</code>.  It can contain
anything.  The only escape it allows is a double backtick, which is replaced
with a single backtick.  Backslashes have no special meaning and are passed on
as-is.

```
`a\`b` -- ERROR: b is past the end of the string
`a\``b` -- Equivalent to "a\\`b"
`Use "\n" for a newline` -- Equivalent to "Use \"\\n\" for a newline"
```

> _For familiarity and readability, it's recommended to use double quotes most
> of the time, and only use backticks for regexes, Windows filepaths, and other
> things susceptible to Leaning Toothpick Syndrome.  AYU nearly didn't include
> nearly raw strings at all, but that would have been unfair to Windows users._

### 3.5. Array

Arrays are delimited by square brackets `[` and `]` and can contain multiple
values, called elements.

Each element may have one comma after it.  Keywords, numbers, and strings (even
if quoted) must have whitespace or a comma between one another.  Arrays,
objects, and macro definitions and invocations can be close and comfy with
anything.

### 3.6. Object

Objects are delimited by curly braces `{` and `}` and contain key-value pairs,
called attributes.  An attribute is a string (the key), followed by a colon `:`,
followed by a value.  Each attribute may have one comma after it.

Keys are parsed exactly the same way as strings, including whether they can be
unquoted or not.  Keys that look like keywords or numbers must be quoted.

Parsers should reject objects with duplicate keys unless they need strict JSON
compatibility.  The order of attributes in an object is non-meaningful, but
should be preserved for readability.

> _If you think you want an object with meaningful order or multiple attributes
> with the same key, use an array of pair-arrays instead._

### 3.7. Comments

A comment starts with a double hyphen `--` and continues to the next newline
byte or the end of the document.  It can appear anywhere whitespace is allowed
except right on the end of a keyword, number, or string, because `-` is a word
character.

Comments are non-meaningful and should not contain type annotations, parsing
directives, or anything of that sort.

> _Don't put a comment right after a `(` because future block comment syntax
> might look like `(-- comment --)`.

### 3.8. Macros

Macros let you name values to reuse later on.  They fill the same role as
backreferences in other data languages.

A macro definition is `(`, then a string (the name), then `:`, then a value,
then `)`.  It can only occur before a value or key or another macro definition.
The definition is effective starting with the `)` and lasting until the
construct that contains it ends.  That construct can be an array, an object, or
the entire document.  A macro definition cannot contain another macro
definition.

A macro invocation is `(`, then a string (the name), then `)`.  A macro
invocation can occur anywhere a value or key can occur.  If the name matches the
name of a currently-defined macro, then the invocation will be replaced with
that definition's value.

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

The default behavior when encountering an undefined macro must be to reject the
document.  A parser may have an API to predefine macros before parsing a
document, but if it's a general-purpose parser it must not predefine any macros
by default.  It may also have an API to parse a document with undefined macros
and fill them in later, in the vein of an SQL prepared statement.

Macro names are parsed the same as strings, including rules for when they have
to be quoted, except that a macro name cannot have a macro definition or
invocation in it.

> _The macro syntax is designed to have as little effect on the rest of the
> grammar as possible, yet have potential to expand into a more complex macro
> system in the future._
>
> _Macros are scoped to limit action at a distance.  If an extension or parser
> API allows multiple documents to be in the same span of text, it might or
> might not want to clear its macro list between documents._

If there is a maximum nesting depth, it should apply to the document both before
and after replacing all macros.  The parser may or may not consider a macro
definition to be one depth layer, but it should not consider a macro invocation
to be one depth layer.

```
 -- Suppose maximum nesting depth is 3
[[
    (foo: [[]] ) -- ERROR: too deep before replacing macros
    (foo) -- ERROR: too deep after replacing macros
]]
```

Macros are transparent, in the sense that they must not be visible to ordinary
program logic.  Whether a macro was used for a particular value or not is
non-meaningful.

### 3.9. Whitespace

The only characters recognized as whitespace for syntactic purposes are space,
newline, carriage return, and tab.  Exotic ASCII whitespaces like form feed and
vertical tab are forbidden outside of quoted strings and comments.  Unicode
whitespace characters are considered word characters, but a Unicode-aware parser
might warn if they are taken that way.

## 4. Non-Syntactic Concerns

### 4.1. Limits

Parsers and printers may limit the length and complexity of documents they are
able to process.  Those limits may depend on the capabilities of the hardware
they're running on, and may be configurable.  Here are some minimum recommended
limits.  General-purpose parsers should try to be at least this generous, if
their hardware is so capable, and documents should try not to exceed these
limits.

- Document length: one billion bytes
- String length (incl. keys and macro names): one billion bytes
- Array length: 120 million elements
- Object length: 60 million attributes
- Total values in one document: 120 million including keys
- Total length of all strings in one document: one billion bytes
- Nesting depth of arrays and objects: 100 levels
- Simultaneously defined macros: 500

If the documents you process start approaching these limits, you should consider
a more efficient solution, such as a streaming JSON parser or a memory-mapped
binary format.

> These limits are intended to restrain the memory usage of a parser in a
> statically-typed language to around 2GB.  A dynamically-typed language could
> potentially require 4GB to 8GB, depending on the document.  The limits are not
> guaranteed to be foolproof.  A maliciously crafted document might be able to
> make a parser consume much more memory while technically obeying them all.

### 4.2. Equality of Values

The current version of AYU does not require all values to be comparable.  Only
strings need to be compared in order to look up macro names and enforce
key-uniqueness in objects.

If a future version or extension of AYU requires general value comparison, this
will be the procedure for it.  It's also recommended for programs that have a
type implementing AYU values to use this for equality.

- If two values have different forms, they are not equal.
- If both values are null, they are equal.
- If both values are bools, they are equal if they're both true or both false.
- If both values are numbers, they are equal if they have the same numerical
  value.  If the distance between two numbers is near the limit of supported
  precision, they may be considered equal even if they are not exactly the same.
  Infinities are equal to infinities of the same sign and nothing else.  All
  NaNs are equal to all other NaNs and nothing else.  Negative zero is equal to
  positive zero.
- If both values are strings, they are equal if their internal contents are the
  same length and have all the same bytes in the same order.
- If both values are arrays, they are equal if they are the same length and all
  of their elements in order are equal.
- If both values are objects, they are equal if they have the same set of keys
  and each attribute with the same key has an equal value, no matter what order
  the objects have their attributes in.

> _Contrary to the usual floating point rules, NaN is considered equal to
> itself.  That's because in this context it is being treated as abstract data,
> not as an undefined mathematical operand._
>
> _The rule that really close numbers can compare equal gives some lenience to
> implementations that have mulitple ways of storing numbers internally; for
> instance when comparing an int64 and a double, they can just convert the int64
> to double without excessive concern about accuracy.  Hopefully no
> implementations take this as license to be sloppy with pure integer
> comparisons._
>
> _A value is always equal to itself, so an implementation that uses reference
> counting to share memory between values is allowed to equate two values that
> have the same memory address, without having to descend into them._

## 5. Compatibility With JSON

AYU is almost a compatible superset of JSON.  An AYU parser can parse most JSON,
and most AYU can be mechanically translated into JSON.  Programs that work with
both AYU and JSON can use the same value implementation for both.  However,
there are some corner cases that merit particular consideration.

### 5.1. Special Numbers

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

### 5.2. Non-UTF-8 Strings

JSON documents and their strings are required to be UTF-8, and many JSON parsers
will reject documents with invalid UTF-8.  Because of this, when translating AYU
to JSON, something must be done with strings that don't contain valid UTF-8.
There are a few options, none of which work for all scenarios.

1. Refuse to translate the document and throw an error.  In theory this prevents
data loss because it should prompt a human operator to find and deal with the
source of the problem.  But in practice, it can cause loss of the entire
document if a machine silently drops it or the operator gives up on fixing it.
2. Censor the strings by replacing invalid bytes or sequences with U+FFFD (the
Replacement Character).  This is the standard and most commonly recommended
approach for sanitizing UTF-8.  However, this loses the distinction between
invalid bytes, leading to potential consequences like filenames that were
different becoming identical and causing the files to overwrite one another.
3. Reinterpret the invalid bytes as Latin-1.  This can cause minor data loss
because the reinterpreted bytes may conflict with legitimate characters between
U+80 and U+FF, but the likelyhood of conflict between full strings is low.
Hopefully a human operator will notice the mojibake and, if they care enough,
recover the original data.
4. Embed the invalid bytes into another character range that is unlikely or
invalid in Unicode text.  This in theory can prevent data loss better than using
the Latin-1 range, but it requires a parser on the other end to agree on the
same embedding scheme, and is less likely to be understood by a human operator.
Every choice of where to embed them also has its own ramifications.  That area
from U+BAD80 to U+BADFF is looking pretty nice.  Let's hope the folks from the
Consortium don't arrest you for using unallocated characters.
5. Replace the string with an array of numbers.  This might preserve the most
data but it only works with serializers that understand what it means, and it
removes the human-readability of non-corrupted parts of the string.
6. Pass the invalid bytes through unaltered.  This produces nonconformant JSON,
but it's the easiest option if you know that any programs that will read it
don't care.  If they do care, this pushes the burden of choice onto them.

If you can't decide which option to pick, pick option 3.

### 5.3. Object Attributes

The JSON spec does not mandate that objects have unique keys.  In practice,
there are almost no JSON documents that have objects with duplicate keys, and
many general-purpose JSON parsers will reject such objects or misbehave on them
(particularly ones that store attributes in a hash map).  An AYU parser may
accept duplicate keys if it needs 100% compatibility with JSON, but this is not
recommended.

Similarly, the JSON spec does not mandate that the order of attributes in
objects is non-meaningful.  Similarly again, there are almost no JSON documents
where attribute order is meaningful, and many JSON parsers don't even expose the
order of attributes in their API (again, like ones that use hash maps).

## 6. Security Considerations

AYU is designed for local storage and human-machine communication.  It's not
designed for network transmission or communication between machines.  It is more
complex and error-prone than JSON, and therefore it should not be used for
untrusted data.  AYU data files should be considered nearly as sensitive as
executable files.

## 7. Non-Normative Notes

### 7.1. Potential Future Features

These are features that may or may not be added to the language in a future
version.  If they are added, they will probably take the given syntax, and other
future features are unlikely to conflict with the syntax.

- Allowing `_` in numbers.
- Rational numbers like `34/56`, expanding the special number syntax.
- Explicit indexes in arrays with `[0:x 1:y]`.
- Block comments like `(-- comment --)`.
- Allowing numeric macro names, to facilitate "prepared statement"-like usage.
- Arithmetic operations that distribute over arrays, so that `(16 * [[1 1] [2 2]
  [3 3]])` yields `[[16 16] [32 32] [48 48]]`.  This would only take place
  inside `( )` so it would not conflict with anything else.
- Including other files as document fragments, strings, or arrays of numbers
  with `(include asdf.ayu)`, `(embed text asdf.txt)`, and `(embed u32le asdf.bin)`.
- The above two features could be implemented as predefined macros in an
  advanced pattern-matching macro system.

### 7.2. Rejected Features

These are features that were considered for AYU at one point, but ultimately
rejected for one reason or another.

- Allowing `null` `true` `false` to be unquoted strings.  It would be nice to
  remove this inconsistency, but any way we could do it would either sacrifice
  JSON compatibility or merely push the surprise further down the road while
  complicating the spec.  Any alternate keywords we could pick would either be
  weird or add another restriction to unquoted strings, which kinda defeats the
  goal we started out with.  `1=1` for true and `0=1` for false would be kinda
  funny though.
- Octal and binary numbers.  Octal is only used for one thing and binary isn't
  valuable enough when hexadecimal is available.
- Hexadecimal floating point numbers.  An earlier version of this language
  included these, but they were deemed too rare to be worth the implementation
  burden in environments that don't have a bulitin parser for them.  Decimal
  format is sufficient to preserve all numbers.  If you want to look at the bit
  patterns of your floats, you might consider serializing them as hexadecimal
  strings instead.
- Single-quoted strings.  If they work the same as double-quoted strings, they
  don't help enough to be worth it.  If they work as nearly raw strings, then
  backticks are better because they are much rarer.  If single quotes do
  something yet different, then we'll have four different string formats, and
  three is already too many.  Furthermore, every language with single-quoted
  strings treats them differently, so any choice we make will be surprising to
  someone.  Declining single-quoted strings lets us put apostrophes in words
  too.  Sorry, single quote preferrers.  YAML, JSON5, and Relaxed JSON are still
  there for you.
- Indentation-controlled strings.  These are quite complicated to implement.
  Putting a long multi-line string in the middle of a data structure also breaks
  up the structure making it harder to read.  It's better to put the string in a
  macro or even an external file.
- Using `:` or `;` in arrays to reduce square bracket usage, allowing `[[a b] [c
  d]]` to be written as `[a:b c:d]` or `[a b; c d]`.  These would not be obvious
  to someone who hasn't read the spec.  They'd also be error-prone, in that
  missing a `:` or `;` would silently change the data to something valid.
- Non-string keys in objects.  Putting aside JSON compatibility, this would be
  easy to parse, but not many deserializers would make use of it.  It wouldn't
  provide much more value than arrays of pair-arrays.  For that matter, how much
  value do ordinary objects even provide over arrays?  Was LISP the way after
  all?  Should we just be using S-expressions for everything?
- Comments using `//` or `#` for similarity with other JSON-likes.  These would
  add more restrictions to unquoted strings that you'd have to remember.  I
  considered using `;` like .ini-style languages do, but `;` looks too similar
  to `:` (it's fine for them because they use `=` for key-value pairs).  `--` is
  also more visually distinct than other alternatives.  It makes comments look
  like negative space, constrasting with the positive space of data.
- Macros using `$var` syntax.  I really wanted this but it added an extra
  restriction to unquoted strings, and it was unclear how to make macro
  definitions look good without causing even more restrictions or being weird to
  parse.  It also would not generalize well to future extensions.
- Document separators to store multiple documents in one file.  Just use an
  array.  For that matter, AYU has no rule that a document has to hog the whole
  file.  You can just put multiple documents separated by whitespace, or embed
  them in another format.

### 7.3. Tips for Serializing and Deserializing

For the most part, AYU does not constrain the behavior (or even existence) of
serializers and deserializers.  Therefore this section is non-normative and
constitutes advice and recommendations, not specifications.

When transforming a value to an item or an item to a value, the mapping between
forms and types can potentially be diverse and complex.  Imagination is the only
limit.

The same form could represent different types.  For instance a string could
represent a chunk of text, or it could select a value of an enum, or it could
name a type.  An array could represent a list of items, or it could specify
which bits are set in a bitfield.  An object could be a struct with a fixed set
of attributes, or it could be a hash map with arbitrary strings for keys.

On the other side, some types could be represented by multiple forms.  For
instance, a matrix-like type might serialize to an array of numbers, or it might
serialize to a string denoting a special matrix, like `id` or `flipx`.  A common
structure could be written as either an array or an object to choose between
brevity and clarity.  A color could be a keyword like `black` or a `#rrggbb`
string or an array of components between 0 and 1.

Here are a few recommendations on how to serialize some non-straightforward item
types that don't seem to fit neatly into value forms.

#### 7.3.1. Dynamically-Typed Items

Like JSON, AYU does not have type annotations.  The type of the item represented
by a value is usually implicit and depends on how the parent item is represented
by the parent value.  Where types are explicit, those types are themselves
represented as values.

The recommended way to serialize an item that could have multiple types is to
use an array of two elements, the first of which is a type name in string form,
and the second of which is the item's content.

```
[f32 3.5]
[app::Settings {foo:3 bar:4}]
[std::vector<int32> [408 502]]
```

There are no standardized type names, because type systems differ between
domains and programming languages.  If you want to store data in a way that's
not domain-specific, you should not use explicit types.  If your type system is
specific to one programming language, you should make most of the type names
identical to their names in that language.

If your program works with a lot of documents stored in separate files, it's
recommended to make each document have a dynamically-typed item at the top level
to keep track of what's what.

#### 7.3.2. Optional and Nullable Types

Types that may or may not contain an item, such as C++'s `std::optional` or
`std::unique_ptr`, can be serialized as an array of one or zero elements.  If
they are the value of an object's attribute, they can alternatively be
represented by the presence or absence of the entire attribute.  If you know
that the contained item can never be `null`, you can also use `null` to indicate
a lack of content.

#### 7.3.3. Binary Data

AYU does not have a blob form.  To represent binary data, you can use an array
of integers or a hexadecimal string.  You could also use a non-UTF-8 string, but
that's incompatible with JSON and is not very readable.  Base64 is not
recommended because it is completely illegible and doesn't save much space over
hexadecimal.  If you have space concerns, use a filepath to an external binary
file.  If human readability isn't important, perhaps reconsider the choice to
use a human-readable data language.

#### 7.3.4. Exotic Floating Point Numbers

If a program wants to preserve signalling NaNs or NaN payloads, the suggested
way to do so is to render the binary representation of the number as a
hexadecimal string starting with `#`, like `#7f800003` to mean a 32-bit
signalling NaN with a payload of 3.  In a pinch this format can also be used as
a stand-in for hexadecimal floats.

#### 7.3.5. Command-Like Items

Use an array whose head is the name of a command and whose tail is its
arguments.  You can build a simple imperative DSL this way.

```
{on_click: [seq
    [add_to_list {sort:natural} storage/bookmarks.list]
    [say "Added to bookmarks."]
]}
```

#### 7.3.6. Formatted Text

Use an array of tokens, each of which is either a literal (string) or a
formatting command (array).  Don't forget to put spaces in the literals.

```
["You have " [unread] " new message" [if_plural [unread] "s"] "."]
```

#### 7.3.7. XML and HTML

These are already fine languages, so you can just put them in strings as-is.
But I suppose you could also do something like `[a {href:example.com} "this is a
" [b "link"]]`.

#### 7.3.8. Pointers, References, Links, etc

The simplest way to serialize pointer-like items is to attach some sort of
integer or string ID to each item that could be pointed to.  Then, a pointer is
represented by one of those IDs.  It is recommended for item IDs to be specified
with an object key named "id".

```
[{
    id: 35
    name: "Ein Stein"
}{
    id: 77
    name: "Heisen Berg"
    father: 35
}]
```

If you don't want to actually store an ID inside each item perpetually, you can
instead generate an external mapping of items to IDs before serializing and use
that instead.

For a maximally general approach, you can serialize a pointer as a "route" or
"path": something that describes how to find the item.  The recommended method
is to encode the route as a string containing an IRI (Unicode URI) reference,
using the following rules:

- Resolve the IRI reference into a full IRI using the current document's IRI as
  a base.  All of the IRI before the fragment tells the deserializer where to
  find the document that contains the referenced item.  If the IRI reference is
  empty except for the fragment, then it refers to the base, i.e. the current
  document.
- If the IRI's fragment is empty or missing, then the pointer points to the
  top-level item of that document.
- A non-empty fragment contains a route composed of segments.  Each segment is
  one of the following:
    - `/` and a key, which says to index an object-like item with that key.
      Keys containing `/`, `+`, `#`, or characters forbidden in IRIs must escape
      those characters in standard URI fashion using percent sequences.  If the
      key is empty, then the key is the empty string.
    - `+` and an optional integer, which says to index an array-like item with
      that zero-based index.  If the index is omitted, it defaults to 1 to make
      it easier to traverse dynamically typed items.
    - The very first segment can be a key without `/`.  If it is, then it gets
      an implicit `+1` both before and after, so that `#key` is a shortcut for
      `#+/key+`.  This helps with documents that are collections of heterogenous
      named items.

Here is an example.

```
[ayu::Collection {
    some_object: [MyObject {
        foo: 50
        bar: [60 70 80 90]
    }]
     -- The following makes some_pointer point to some_object.bar[2].
     -- #some_object/bar+2 is equivalent to #+1/some_object+1/bar+2
     -- # -> [ayu::Collection {some_object:[...] ...}]
     -- #+1 -> {some_object: [MyObject {...} ...]}
     -- #+1/some_object -> [MyObject {foo:50 bar:[...]}]
     -- #+1/some_object+1 -> {foo:50 bar:[60 70 80 90]}
     -- #+1/some_object+1/bar -> [60 70 80 90]
     -- #+1/some_object+1/bar+2 -> 80
    some_pointer: [int32* #some_object/bar+2]
     -- The following points to an item in another file.
    another_pointer: [AnotherObject* /folder/file.ayu#target]
]]
```

Converting a route like this into a pointer is straightforward: just follow it
one segment at a time.  However, converting a pointer to a route can be quite a
heavyweight operation.  In the general case it will require scanning lots of
data to find the target.  It might be a good idea to generate an index mapping
routes to addresses before serializing.

Alternative syntaxes include:

- JSONPath: `"$[1].some_object[1].bar[2]"`
- Arrays: `[# 1 some_object 1 bar 2]`

#### 7.3.9. Graph-Like Data Structures

The AYU language doesn't have any builtin syntax to represent graphs.  If you
need to serialize a graph-like data structure in AYU, here are three possible
approaches.

1. Put all the nodes of the graph into a flat array or object, and represent all
the edges as indexes or keys for that array or object.  This is the recommended
way if your graph is a formal self-contained "graph" type of item.

2. While serializing, put the address of any item that could be a graph node
into a hash set.  If you're about to serialize an item that's already in the
set, then serialize it as a reference instead of as a fully owned item.  This
approach might be easier if the graph is "a bunch of heterogenous structures all
over the place linking to one another with no clear sense of ownership."

3. Find a spanning tree of the graph, represent edges in the tree with nesting,
and edges not in the tree like references.  This is equivalent to approach 2
except more nerdy.

A language that natively supports cyclical data could have its processor do this
work for you.  AYU leaves it as a job for serializers, under the assumption that
most data does not (or does not need to) have cycles.  Allowing values to be
cyclical would force all deserializers and printers to detect those cycles to
avoid infinite loops.  Without cyclical values, the only component that needs to
be aware of cycles is the serializer, and it only needs to consider them in
those item types that it knows ahead of time can be cyclical.
