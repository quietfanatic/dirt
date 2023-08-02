AYU DATA LANGUAGE
=================

The AYU data language represents structured data in a simple readable format.

### File format

An AYU file is UTF-8 text, with LF (unix-style) newlines.  Writers should not
emit a byte-order mark, but readers should accept one.

### Forms (AKA types)

Items in AYU can have these forms:

##### Null

Represented by the keyword `null`.

##### Bool

Represented by the keywords `true` and `false`.

##### Number

An integer or floating-point number.

Numbers can start with a `+` or a `-`.  They can be decimal numbers with the
digits `0` through `9`.  They can also be hexadecimal numbers starting with `0x`
or `0X` and containing `0`-`9`, `a`-`f`, and `A`-`F`.  No other bases are
supported.  Numbers may start with leading 0s, and are interpreted as decimal
numbers, NOT as octal numbers.

Numbers may be followed by a decimal point (`.`) and then more digits of their
base.  They may then be followed by an `e` or `E` for decimal numbers and a `p`
or `P` for hexadecimal numbers, followed by an optional sign and then decimal
digits.  The exponent is always in decimal, even if the rest of the number is in
hexadecimal.

Numbers cannot start or end with a `.`.  `.5` and `5.` are not valid numbers.

Three special numeric values are supported: `+inf`, `-inf`, and `+nan`.  The
sign is required on all of these.  Without the sign, they would be considered
strings.  There is only one +nan, which is the canonical NaN for the platform.
-0.0 is preserved but considered equal to +0.0.

The reference implementation supports floating-point numbers of double precision
and all integers between -2^63 and 2^63-1.

##### String

A UTF-8 string.  Double quotes (`"`) delimit a string, though in many cases, the
quotes are optional.  Quoted strings may contain multiple lines and any UTF-8
characters except for unescaped `"` and `\\`.  The following escape sequences
are supported in quoted strings.
- `\\b` = Backspace
- `\\f` = Form Feed (nobody uses this)
- `\\n` = Newline (LF)
- `\\r` = Carriage Return (CR)
- `\\t` = Tab
- `\\"` = literal quote character
- `\\\\` = literal backslash
- `\\/` = literal slash
- `\\xXX` = UTF-8 byte with a two-digit hexadecimal value, which may be part of
  a multibyte UTF-8 sequence, but must not be an unmatched leading or
  continuation byte.
- `\uXXXX` = A UTF-16 code unit, which may be part of a surrogate pair, but
  must not be a lone surrogate.  A sequence of multiple adjacent `\uXXXX`s will
  be converted together from UTF-16 to UTF-8.

Some of these escape sequences may be obscure or useless, but they're included
for compatibility with JSON.

A string does not have to be quoted if it:
- is not `null`, `true`, `false`, or `//`
- starts with a letter or one of `_`, `/`, `?`, or `#`
- only contains the following:
    - ASCII letters, numbers, or underscores
    - any of these symbols: `!`, `$`, `%`, `+`, `-`, `.`, `/`, `<`, `>`, `?`, `@`,
      `^`, `_`, `~`, `#`, `&`, `*`, `=`
    - the sequence `::` (for C++ namespaces)

The following characters are reserved and are not valid for either syntax or
unquoted strings
- `\\`, `\`` (backtick), `(`, `)`, `'` (single quote), `;` (semicolon)

##### Array

Arrays are delimited by square brackets (`[` and `]`) and can contain multiple
items, called elements.  Commas are allowed but not required between items.

##### Object

Objects are delimited by curly braces (`{` and `}`) and contain key-value pairs,
called attributes.  A key-value pair is a string (following the above rules for
strings), followed by a colon (`:`), followed by an item.  Commas are allowed
but not required between attributes.  Keys named `null`, `true`, or `false` must
be quoted, just as with strings.  The order of attributes in an object should be
preserved for readability, but should not be semantically significant.  If you
think you want an object with order-significant attributes, use an array of
pairs instead.

### Other Syntax

##### Comments

Comments start with `--` and continue to the end of the line.  There are no
multi-line comments, but you can fake them with a string in an unused shortcut.
Readers must not allow comments to change the sematics of the document.

Comments cannot start in the middle of an unquoted string, because `-` is a
valid character for unquoted strings.

##### Shortcuts

Shortcuts (known as backreferences in some other DLs) can be declared with an
ampersand (`&`), followed by a string, followed by either an item or a colon and
an item.  If there is no colon, a copy of the item is left in the declaration's
place (like in YAML).  If there is a colon, the whole declaration is discarded
at that point.  (This is so you can declare shortcuts ahead of time at the
beginning of a file or block.)

Shortcuts can be used later with an asterisk (`\*`) followed by a string.
Whatever item the shortcut was declared with earlier will be used in its
place.  Shortcuts can be used as the keys in objects if they refer to strings.
Shortcuts are syntax only; they are semantically invisible and cannot be
recursive.  Shortcut names are global to the file or string being parsed, and
can only be used after they are declared.
```
[1 &a 2 3 *a] -- Equivalent to [1 2 3 2]
[1 &a:2 3 *a] -- Equivalent to [1 3 2]
```

### Notes

AYU is designed to be compatible with JSON.  A valid JSON document is also a
valid AYU document, and a valid AYU document can be rewritten as a valid JSON
document, with the following caveats:
- JSON does not support `+nan`.  Serializers should use `null` instead, and
  deserializers should accept `null` as a floating point number equivalent
  to `+nan`.  This may potentially lead to ambiguities for certain types such as
  C++'s `std::optional<float>`.
- JSON does not support `+inf` and `-inf`.  Instead, these should be written
  with a number outside of double floating-point range for JSON (canonically
  `1e999` and `-1e999`).

Unlike some other data languages, AYU does not have type annotations.  The
typical way to represent an item that could have multiple types is to use an
array of two elements, the first of which is a type name and the second of which
is the value.
```
[float 3.5]
[app::Settings {foo:3 bar:4}]
[std::vector<int32> [408 502]]
```

The AYU data language does not natively support references, but the AYU
serialization library supports references in the form of IRI strings.  As an
example,
```
[ayu::Document {
    some_object: [MyObject {
        foo: 50
        bar: [60 70 80 90]
    }]
     -- The following makes some_pointer point to some_object.bar[2].
     -- # -> the current document
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
to a separate binary file.

The unquoted string rules were chosen so that most relative URLs do not have to
be quoted.  Full URLs with a scheme still have to be quoted because they contain
a colon.

I chose `--` for comments because `//` and `#` can start URLs and `;` looks too
similar to `:`.  `--` also stands out more than the other alternatives.
