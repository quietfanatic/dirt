AYU DATA LANGUAGE
=================

The AYU data language stores structured data in an easy-to-read format.  It is a
superset of and nearly isomorphic to JSON: a valid JSON document is also a valid
AYU document, and most AYU documents can be directly translated into JSON
documents.

_Note: The only AYU documents that can't be easily translated into JSON are
those containing non-UTF-8 strings.  Translating such documents will require
either censoring the strings, reinterpreting them as Latin-1, or replacing them
with arrays of integers._

### File format

An AYU document is made of text in any ASCII-compatible encoding.  Writers
should not emit a UTF-8 byte-order mark, but readers should accept one.

_Note: It's expected that AYU documents will be mainly UTF-8, but their strings
are not guaranteed to be UTF-8.  If you need a strict UTF-8 string, you must
validate it while deserializing it.  Validating the entire document ahead of
time is not sufficient, since strings can contain arbitrary escaped bytes._

A document contains one item of any form.  It is not restricted to an array or
an object.

_Note: JSON hasn't had this restriction since 2014._

Readers and writers may enforce a maximum nesting depth.  If they do, its
default value must be at least 10 and should be at least 50.

### Item forms

Every item has one of these forms: null, bool, number, string, array, object.

_Note: These are not "types".  These are merely syntactic forms, and do not
correspond directly to the types of the data being serialized or deserialized.
The actual types of the data are usually implicit, with the interpretation of an
item being dependent on its parent item.  When types are explicit, those types
must themselves become part of the data.  See the section below titled "How are
some exotic types represented in AYU?"_

_The same form may represent different types.  For instance a string could
represent a chunk of text, or it could represent a value of an enum.  An array
could represent a list of values, or it could specify which bits are set in a
bitfield._

_On the other side, some types can be represented by multiple forms.  For
instance, a matrix-like type might become an array of numbers, or it might
become a string denoting a special matrix, like "id" or "flipx".  A common
structure could be written as either an array or an object to choose between
brevity and clarity._

#### Null

Indicated by `null`.

#### Bool

Indicated by `true` or `false`.

#### Number

Decimal numbers are composed of:

- an optional sign `+` or `-`,
- a run of decimal digits
- an optional decimal point `.`, followed by a run of decimal digits,
- an optional exponent `e` or `E`, followed by an optional sign, followed by a
  run of decimal digits which indicates a power of ten.

Numbers cannot start or end with a `.`.  `.5` and `5.` are not valid numbers.

_Note: The language does not differentiate between integers and real numbers.
There is no semantic difference between `3` and `3.0` for instance.  A reader
may optimize them differently, but they must have the same meaning.  Decimal
numbers may have any number of leading zeroes, and they are still interpreted as
decimal, not octal._

Hexadecimal numbers are composed of:

- an optional sign, `+` or `-`,
- the prefix `0x` or `0X`,
- a run of hexadecimal digits.

_Note: No other bases are currently supported.  An earlier version of this
language included hexadecimal floating point numbers, but they were deemed too
rare to be worth the implementation burden in environments that don't have a
bulitin parser for them._

There are three special numeric values: `+inf`, `-inf`, and `+nan`.

_Note: JSON does not support these special numbers.  When converting an AYU
document to a JSON document, they should be converted to the case-sensitive
quoted strings `"NaN"`, `"Infinity"`, and `"-Infinity"`, and deserializers should
accept these strings as alternative values for floating point types.  These
names seem to be the most common choices for these special values, as they are
the JavaScript names for them._

What ranges and precisions of numbers are supported is implementation-defined.

_Note: The reference C++ implementation supports floating-point numbers of
double precision and all integers between -2^63 and 2^63-1._

#### String

Double quotes (`"`) delimit a string, though sometimes the quotes are optional.
Quoted strings may contain multiple lines and any characters except for
unescaped `"` and `\\`.  The following escape sequences are supported in quoted
strings.

- `\\n` = Newline (LF), equivalent to `\\x0a`
- `\\r` = Carriage Return (CR), equivalent to `\\x0d`
- `\\t` = Tab, equivalent to `\\x09`
- `\\"` = literal quote character
- `\\\\` = literal backslash
- `\\xXX` = A byte with a two-digit hexadecimal value, which may be part of
  a multibyte character.  Strings are not required to be valid UTF-8.

_Note: Non-UTF-8 strings are incompatible with strict JSON._

In addition, these escape sequences are supported for compatibility with JSON.

- `\\b` = Backspace, equivalent to `\\x08`
- `\\f` = Form Feed, equivalent to `\\x0b`
- `\\/` = literal slash, equivalent to just `/`
- `\\uXXXX` = A UTF-16 code unit, which may be part of a surrogate pair, but
  must not be a lone surrogate.  A sequence of multiple adjacent `\\uXXXX`s will
  be converted together from UTF-16 to UTF-8.

A backslash followed by anything else is an error.

##### Unquoted strings (sometimes called words)

A string does not have to be quoted unless:

- it is a keyword (`null`, `true`, or `false`), or
- it starts with a digit, `+`, `-`, or a `.` followed by a digit, or
- it contains any characters that:
    - are ASCII whitespace or control characters
    - look like delimiters: `[` `]` `{` `}` `(` `)`
    - look like separators: `,`, `;`, a single `:` (double `::` is allowed if
      what follows it could also be an unquoted string)
    - look like quotes or escapes: `"` (double quote), `'` (single quote),
      <code>\`</code> (backtick), `\\` (backslash)

Conversely, any of these are allowed in unquoted strings:

- ASCII letters and digits
- `!` `#` `$` `%` `&` `\*` `+` `-` `.` `/` `<` `=` `>` `?` `@` `^` `\_` `|` `~`
- `::`
- non-ASCII bytes

An unquoted string must not be immediately followed by a `"` without whitespace,
because that is looks like a mistake.

_Note: To avoid having to examine megabytes of Unicode tables, any non-ASCII
bytes are allowed in unquoted strings.  Please do not abuse this privilege.  If
it looks like whitespace or syntax, quote it._

_Note: The unquoted string rules were chosen to include most relative IRI
references.  Full IRIs with a scheme still have to be quoted because they
contain a colon.  Some characters that are not used for syntax are reserved for
future extensions or simply because they look like they could be syntax.  Double
colon is allowed as a special exemption for namespaces in C++ and related
languages._

_Note: Unlike Relaxed JSON (rjson), backslashes cannot be used to escape
characters in unquoted strings.  Unquoted strings are always identical to their
contents and can be memcpyed straight from the source file (or even, if you're
brave, borrowed)._

#### Array

Arrays are delimited by square brackets `[` and `]` and can contain multiple
items, called elements.  Commas are allowed but not required between items, and
a final comma is allowed.  Multiple commas in a row are not allowed, nor is an
initial comma.

#### Object

Objects are delimited by curly braces `{` and `}` and contain key-value pairs,
called attributes.  An attribute is a string (the key), followed by a colon `:`,
followed by an item (the value).  All attributes must have unique keys.  Comma
allowance is the same as for arrays.  The order of attributes in an object
should be preserved for readability, but should not be semantically significant.
If you think you want an object with order-significant attributes, use an array
of pair-arrays instead.

Keys use the exact same parsing rules as strings, including whether they can be
unquoted or not.  Keys that look like keywords or numbers must be quoted.

### Other Syntax

#### Comments

Comments start with `--` and continue to the end of the line.  There are no
multi-line comments, but you can fake them with a string in an unused shortcut.
Readers must not allow comments to change the meaning of the document.

Comments cannot start in the middle of an unquoted string, because `-` is a
valid character for unquoted strings.

_Note: Comments use `--` because `//` and `#` would add more restrictions to
unquoted strings and because `;` looks too similar to `:`.  `--` is also
visually distinct, making comments look more like negative space._

#### Macros

Macros let you name items to reuse later on.  They are only for convenience of
writing documents, and are completely invisible to the data once read.

A macro definition is `(`, then a string, then `:`, then an item, then `)`.  The
definition takes effect starting at the `)` and lasting until the end of the
document or until redefined.  A macro definition can only occur before an item
or key or another macro definition.  It cannot be recursive and cannot contain
another macro definition.

A macro invocation is `(`, then a string, then `)`.  If the string matches the
left side of a currently-defined macro, then the invocation will be replaced
with the right side of that macro.

```
(foo: 1)
(foo: [(foo) (foo)]) -- (foo) is now [1 1]
[1 (a:2) 3 (a)] -- Reduces to [1 3 2]
{(k:arf) (k):(k)} -- {arf:arf}
```

Macro names are parsed the same as strings, including rules for quoting.

_Note: The macro syntax is designed to have as little effect on the rest of the
grammar as possible, yet have potential to expand into a more complex macro
system in the future._

If there is a maximum nesting depth, it should apply to the document both before
and after replacing all macros.

```
 -- Suppose maximum nesting depth is 3
[[ (foo: [[]] ) ]] -- ERROR: too deep before replacing macros
[[ (foo) ]] -- ERROR: too deep after replacing macros
```

### Other notes

#### How are some exotic types represented in AYU?

Unlike some other data languages, AYU does not have type annotations.  The
recommended way to represent an item that could have multiple types is to use an
array of two elements, the first of which is a type name (in String form) and
the second of which is the value.

```
[float 3.5]
[app::Settings {foo:3 bar:4}]
[std::vector<int32> [408 502]]
```

Types that may or may not contain a value, such as `std::optional`, are
typically represented as an array of one or zero elements.  If they are a value
in an object, they can alternatively be represented by the presence or absence
of the entire attribute.

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

AYU does not have a form for binary data.  To represent binary data, you can use
an array of integers, or a string of hexadecimal digits, or a filename pointing
to a separate binary file.  You can also use a non-UTF-8 string, but that's
incompatible with JSON.

#### Raku-style grammar

``` raku
token ws { \s* ['--' \N* \s*]* }

rule item {
    | 'null' | <bool> | <number> | <string> | <array> | <object>
    | <macro-invocation> | <macro-definition> <item>
}

token bool { 'true' | 'false' }

token number { <decimal> | <hexadecimal> }

token decimal {
    <[+-]>? <[0..9]>+
    ['.' <[0..9]>+]?
    [['e' | 'E'] <[+-]>? <[0..9]>+]?
}

token hexadecimal { <[+-]>? '0' <[xX]> <hex>+ }

token hex { <[0..9a..fA..F]> }

token string { <quoted-string> | <unquoted-string> }

token quoted-string { '"' [<-["\\]> | '\\' <escape>]* '"' }

token escape { <[bfnrt"/\\]> | 'x' <hex> ** 2 | 'u' <hex> ** 4 }

token unquoted-string {
    [
        [ <[a..zA..Z!#$%&*/<=>?@^_|~\x80..\xff]> | '.' <!before: <[0..9]>> ]
        [ <[0..9a..zA..Z!#$%&*+-./<=>?@^_|~\x80..\xff]>]*
    ] % '::'
}

rule array { '[' [<item> ','? ]* ']' }

rule object { '{' [<key> ':' <item> ','? ]* '}' }

rule key { <string> | <macro-invocation> | <macro-definition> <string> }

rule macro-invocation { '(' <string> ')' }

rule macro-definition { '(' <string> ':' <item> ')' }
```
