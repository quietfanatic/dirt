AYU DATA LANGUAGE
=================

The AYU data language stores structured data in an easy-to-read format.

AYU does not stand for anything; it is just a cute-sounding name sitting in the
highly saturated namespace of data languages.

AYU is a near-compatible superset of JSON: a valid JSON document is also a valid
AYU document, and most AYU documents can be directly translated into JSON
documents.  Of all the known JSON descendants it is most similar to Relaxed JSON
(rjson).

_Note: The only AYU documents that can't be easily translated into JSON are
those containing non-UTF-8 strings.  Translating such documents will require a
modification such as censoring the strings, reinterpreting them as Latin-1, or
replacing them with arrays of integers._

### Overview

- Null is `null`.
- Bools are `true` and `false`.
- Numbers can be decimal or hexadecimal.  You can write infinity with `1/0` and
  NaN with `0/0`.
- Strings use `"`.  Most single-word strings don't have to be quoted.
- Arrays use `[ ]` and contain elements separated by space and/or `,`.
- Objects use `{ }` and contain `key:value` attributes.
- Comments start with `--`.
- Macros are defined with `(name:value)` and used with `(name)`.

### File format

An AYU document is made of text in any ASCII-compatible encoding.  Writers
should not emit a UTF-8 byte-order mark, but readers should accept one.

_Note: It's expected that AYU documents will be mainly UTF-8, but their strings
are not guaranteed to be UTF-8.  If you need a strict UTF-8 string, you must
validate it while deserializing it.  Alternatively, you can validate the entire
document ahead of time as long as you detect runs of \xXX escapes and validate
them too._

A document contains one item of any form.  It is not restricted to an array or
an object.

_Note: JSON hasn't had this restriction since 2014._

Every item has one of these forms: null, bool, number, string, array, object.

_Note: These are not "types".  These are merely syntactic forms, and do not
correspond directly to the types of the data being serialized or deserialized.
As in JSON, the actual types of the data are usually implicit, with the
interpretation of an item being dependent on its parent item.  When types are
explicit, those types must themselves become part of the data.  See the section
below titled "How are some exotic types represented in AYU?" for more on
explicit types._

_The same form may represent different types.  For instance a string could
represent a chunk of text, or it could represent a value of an enum, or it could
be the name of a type.  An array could represent a list of values, or it could
specify which bits are set in a bitfield.  An object could be a struct with a
fixed set of attributes, or it could be a hash map with arbitrary strings for
keys._

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
There is no semantic difference between `3` and `3.0`.  A reader may optimize
them differently, but they must have the same meaning.  Decimal numbers may have
any number of leading zeroes, and they are still interpreted as decimal, not
octal._

Hexadecimal numbers are composed of:

- an optional sign `+` or `-`,
- the prefix `0x` or `0X`,
- a run of hexadecimal digits.

There are three special numbers with six names:

- Positive infinity is written as `1/0` or  `+1/0`.
- Negative infinity is written as `-1/0`.
- NaN (Not a Number) is written as `0/0`, `+0/0`, or `-0/0`.

These are matched exactly and no other variations are accepted.  `0/0.0` and
`2/0` are not allowed.

_Note: These names were chosen because they start the same way as ordinary
numbers, so they don't add any restrictions to unquoted words._

_Note: JSON does not support these special numbers.  When translating AYU to
JSON, they should be changed to the case-sensitive quoted strings `"Infinity"`,
`"-Infinity"`, and `"NaN"`.  Deserializers should accept these strings as values
for floating point types.  These names seem to be the most common choices for
JSON, because Javascript recognizes them in its implicit coercions.  Sometimes
`null` is used for NaN, but this is not recommended because many JSON writers
render both NaN and infinities as `null`._

What ranges and precisions of numbers are supported is implementation-defined.
Whether negative zero and negative NaN are preserved is implementation-defined.

_Note: The reference C++ implementation supports floating-point numbers of
double precision and all integers between -2^63 and 2^63-1.  It preserves
negative zero but not negative NaN._

#### String

Double quotes (`"`) delimit a string, though sometimes the quotes are optional.
Quoted strings may span multiple lines and have any characters except for
unescaped `"` and `\\`.  The following escape sequences are supported in quoted
strings.

- `\\n` = Newline (LF), equivalent to `\\x0a`
- `\\r` = Carriage Return (CR), equivalent to `\\x0d`
- `\\t` = Tab, equivalent to `\\x09`
- `\\"` = literal quote character
- `\\\\` = literal backslash
- `\\xXX` = A byte with a two-digit hexadecimal value, which may be part of
  a multibyte character.  It is recommended but not required for strings to be
  valid UTF-8 after replacing all escapes.

_Note: Non-UTF-8 strings are incompatible with conformant JSON, but we support
them because POSIX filenames can have arbitrary bytes that aren't valid UTF-8._

Also these escape sequences are supported for compatibility with JSON.

- `\\b` = Backspace, equivalent to `\\x08`
- `\\f` = Form Feed, equivalent to `\\x0b`
- `\\/` = literal slash, equivalent to just `/`
- `\\uXXXX` = A UTF-16 code unit, which may be part of a surrogate pair, but
  must not be a lone surrogate.  A sequence of multiple adjacent `\\uXXXX`s will
  be converted together from UTF-16 to UTF-8.

A backslash followed by anything else (including a line ending) is an error.

##### Unquoted strings

An unquoted string is also called a word, and it has one or more word characters
with some restrictions on how it can start.

These are considered word characters:

- ASCII letters and digits
- `!` `#` `$` `%` `&` `\*` `+` `-` `.` `/` `<` `=` `>` `?` `@` `^` `\_` `|` `~`
- non-ASCII bytes (value >= 128)
- double-colon `::` if it has a non-double-colon word character on each side.

Conversely, these are not word characters:

- ASCII whitespace or control characters
- things that look like delimiters, separators, quotes, or escapes:
    - `[` `]` `{` `}` `(` `)` `,` `;` `:` `"` `'` <code>\`</code> `\\`

Words cannot start with any of:

- a digit
- any of `+` `-` `.` followed by a digit
- `--` (which starts a comment)

Finally, the whole word cannot be any of the keywords `null` `true` `false`.

_Note: To avoid having to import megabytes of Unicode tables, any non-ASCII
bytes are allowed in unquoted strings.  Please do not abuse this privilege.  If
it looks like whitespace or syntax, quote it._

_Note: The unquoted string rules were chosen to include most relative IRI
references.  Full IRIs with a scheme still have to be quoted because they
contain a colon.  Some characters that are not used for syntax are reserved for
future extensions or simply because they look like they could be syntax.  Double
colon is allowed as a special affordance for type names in C++ and related
languages.  There is a chance this affordance will disappear before this spec
becomes too public to change._

#### Array

Arrays are delimited by square brackets `[` and `]` and can contain multiple
items, called elements.

Commas are allowed but not required between items, and a final comma is allowed.
Multiple commas in a row are not allowed, nor is an initial comma.  Keywords,
numbers, and strings (even if quoted) must have whitespace or a comma between
one another.  Arrays, objects, and macro definitions and invocations can be
close and comfy with anything.

Readers and writers may enforce a maximum nesting depth for arrays and objects.
If they do, its default value must be at least 10 and should be at least 50.

#### Object

Objects are delimited by curly braces `{` and `}` and contain key-value pairs,
called attributes.  An attribute is a string (the key), followed by a colon `:`,
followed by an item (the value).  Comma and whitespace rules are the same as for
arrays.  All attributes must have unique keys.  The order of attributes in an
object should be preserved for readability, but should not be semantically
significant.  If you think you want an object with order-significant attributes
or multiple attributes with the same key, use an array of pair-arrays instead.

Keys use the exact same parsing rules as strings, including whether they can be
unquoted or not.  Keys that look like keywords or numbers must be quoted.

#### Comments

Comments start with `--` and continue to the end of the line.  Readers must not
allow comments to change the meaning of the document.

A comment can appear anywhere whitespace is valid except right on the end of a
number or word, because `-` is a word character.

#### Macros

Macros let you name items to reuse later on.  They are only for convenience of
writing documents, and are not semantically visible after the document has been
read.

_Note: The restriction about "not semantically visible" allows readers to
optimize data differently, such as by using reference counting to reduce memory
churn, but the data must not behave differently depending on whether macros were
used or not._

A macro definition is `(`, then a string, then `:`, then an item, then `)`.  The
definition takes effect starting at the `)` and lasting until the end of the
document or until redefined.  A macro definition can only occur before an item
or key or another macro definition.  It cannot contain another macro definition.

A macro invocation is `(`, then a string, then `)`.  If the string matches the
left side of a currently-defined macro, then the invocation will be replaced
with the right side of that macro.  A macro invocation can occur anywhere an
item or key can occur.

Macros are replaced eagerly.  They cannot be recursive and can be redefined.  A
macro can be used as a key in an object if its value is a string.  It cannot be
used as the name of another macro.

```
(foo: 1)
(foo: [(foo) (foo)]) -- (foo) is now [1 1]
[1 (a:2) 3 (a)] -- Reduces to [1 3 2]
{(k:arf) (k):(k)} -- {arf:arf}
```

Macro names are parsed the same as strings, including rules for when they have
to be quoted.

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

### Non-normative notes

#### Potential future features

- Raw strings surrounded by triple-single-quotes, like `'''ain't'''`.  Or maybe
  single-single-quotes with any number of parens inside.
- Variable-length unicode code point escapes with `\U{XXXX...}`.
- Multi-character byte escapes with `\x[XX XX ...]`.
- Explicit indexes in arrays with `[0:x 1:y]`.
- Non-string keys in objects (which would probably break JSON compatibility).
- Block comments like `(-- comment --)`.
- Arithmetic operations that distribute over arrays, so that `(16 * [[1 1] [2 2]
  [3 3]])` yields `[[16 16] [32 32] [48 48]]`.  This would only take place
  inside `( )` so it would not conflict with anything else.

#### Rejected features

- Octal and binary numbers.  Octal is rarely used and binary isn't important
  enough when hexadecimal is present.
- Hexadecimal floating point numbers.  An earlier version of this language
  included these, but they were deemed too rare to be worth the implementation
  burden in environments that don't have a bulitin parser for them.
- Single-quoted strings.  They don't provide much that double-quoted or raw
  strings don't provide.
- Indentation-controlled strings.  Quite complicated to implement.  Putting a
  long multi-line string in the middle of a data structure also breaks up the
  structure making it harder to read.  It's better to put the string in a macro.
- Using `:` or `;` in arrays to reduce square bracket usage, allowing `[[a b] [c
  d]]` to be written as `[a:b c:d]` or `[a b; c d]`.  These would not be obvious
  to someone who only knows JSON and hasn't read this spec.  They'd also be
  error-prone, in that missing a `:` or `;` would silently change the data to
  something valid.
- Attributes with no value in objects, like `{a:b c d:e}` to mean `{a:b c:null
  d:e}`.  This is rarely needed and would make it so that missing a `:` would
  silently change the data.
- Comments using `//` or `#` for similarity with other JSON-likes.  These would
  add extra restrictions to unquoted strings that you'd have to remember.  As
  is, the only characters that you have to watch out for are the number- related
  `+`, `-`, and `.`.  I considered using `;` like in .ini-style languages, but
  `;` looks too similar to `:` (it's fine for them because they use `=` for
  key-value pairs).  `--` is also more visually distinct than other
  alternatives.  It makes comments look like negative space, constrasting with
  the positive space of data.
- Macros using `$var` syntax.  I really wanted this but it added an extra
  restriction to unquoted strings, and it was unclear how to make macro
  definitions look good without causing even more restrictions or being weird to
  parse.

#### How are some exotic types represented in AYU?

##### Dynamically-typed things

Unlike some other data languages, AYU does not have type annotations.  The
recommended way to represent an item that could have multiple types is to use an
array of two elements, the first of which is a type name (in string form) and
the second of which is the value.

```
[float 3.5]
[app::Settings {foo:3 bar:4}]
[std::vector<int32> [408 502]]
```

##### Optional types

Types that may or may not have a value, such as C++'s `std::optional` or
`std::unique_ptr`, are typically represented as an array of one or zero
elements.  If they are the value of an object's attribute, they can
alternatively be represented by the presence or absence of the entire attribute.
If you know that the underlying type can never be `null`, you can also use
`null` to represent no value.

##### Binary data

AYU does not have a blob form.  To represent binary data, you can use an array
of integers, or a string of hexadecimal digits, or a filename pointing to a
separate binary file.  You can also use a non-UTF-8 string, but that's
incompatible with JSON.

##### Pointers, references, links, etc

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

#### Raku-style grammar

This is untested and may have bugs.

``` raku

rule item {
    | <null> | <bool> | <number> | <string> | <array> | <object>
    | <macro-invocation> | <macro-definition> <item>
}

token null { 'null' }

token bool { [ 'true' | 'false' ] }

token number { <decimal> | <hexadecimal> | <special-number> }

token decimal {
    <[+-]>? <[0..9]>+
    ['.' <[0..9]>+]?
    [['e' | 'E'] <[+-]>? <[0..9]>+]?
}

token hexadecimal { <[+-]>? '0' <[xX]> <.hex>+ }

token hex { <[0..9a..fA..F]> }

token special-number { <[+-]>? <[01]> '/0' }

token string { <quoted-string> | <unquoted-string> }

token quoted-string { '"' [<-["\\]> | '\\' <escape>]* '"' }

token escape { <[bfnrt"/\\]> | 'x' <.hex> ** 2 | 'u' <.hex> ** 4 }

token unquoted-string {
    [ <.word-start> <.word-char>* ] !~ ['null'|'true'|'false']
}

token word-start {
    | <[a..zA..Z!#$%&*/<=>?@^_|~\x80..\xff]>
    | '.' <!before <[0..9]>>
    | <[+-]> <!before <word-char>>
}

token word-char {
    `::`? <[0..9a..zA..Z!#$%&*+-./<=>?@^_|~\x80..\xff]>
}

rule array { '[' [<item> ','? ]* ']' }

rule object { '{' [<key> ':' <item> ','? ]* '}' }

rule key { <string> | <macro-invocation> | <macro-definition> <string> }

rule macro-invocation { '(' <string> ')' }

rule macro-definition { '(' <string> ':' <item> ')' }

 # Special token inserted between things in 'rule' declarations
token ws {
     # Require whitespace between word characters
    [ <!after <word-char>|'"'> | <!before <word-char>|'"'> | <.ws-char> ]
    [ <.ws-char>* ['--' \N* <.ws-char>*]* ]
}

token ws-char { <[ \f\n\r\t\v]> }
```
