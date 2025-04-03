AYU
===

#### All Your data is belong to yoU

This library includes:
 - A readable and writable structured data language
 - A serialization and reflection system for C++
 - A linked resource management system

This is under heavy development and a couple features aren't fully implemented
yet.  Use at your own risk.

#### AYU Data Language

The AYU data language is similar to JSON but with the following differences:
 - Commas are not required.
 - Quotes are not required around strings that don't contain whitespace or
   syntactic characters, excepting `null`, `true`, and `false`.
 - Comments are allowed starting with `//` and going to the end of the line.
 - There are shortcuts (like backreferences in YAML).  Preceding an item with
   &name will allow a copy of the same item to be inserted later with `*name`.
   Using `&name` followed by a `:` and then an item will declare a shortcut to
   that item without inserting it into the document at that point.
 - Hexadecimal numbers are allowed starting with `0x`.
 - Special floating point numbers `+inf`, `-inf`, and `+nan` are available.
 - The order of attributes in objects is generally preserved, but should not be
   semantically significant.

See data/ayu-data-language.md for more details.

#### Serialization library

FULL DOCUMENTATION PENDING

The serialization system is designed to stay out of your way when you don't need
it and do as much as possible when you do.  Here is an example of its usage.

```
enum class Pie {
    Apple,
    Banana,
    Cherry
};

struct Bakery {
    std::vector<Pie> pies;
    Pie* display;
    void set_display (Pie*);
};

AYU_DESCRIBE(Pie,
    values(
        value("apple", Pie::Apple),
        value("banana", Pie::Banana),
        value("cherry", Pie::Cherry)
    )
)

AYU_DESCRIBE(Bakery,
    attrs(
        attr("pies", &Bakery::pies),
        attr("display", value_funcs<Pie*>(
            [](const Bakery& v){ return v.display; }
            [](Bakery& v, Pie* const& m){ v.set_display(m); }
        )
    ),
)

int main () {
    Bakery smiths;
    smiths.pies = {Pie::Cherry, Pie::Banana, Pie::Apple};
    smiths.on_display = &smiths.pies[2];

    std::cout << ayu::item_to_string(&smiths) << std::endl;
     // Prints {pies:[cherry banana apple] display:#/pies/2}

    item_from_string(&smiths,
        "{pies:[apple cherry banana banana] display:#/pies/1}"
    );
     // bakery now has 4 pies and is displaying a cherry pie.
}

```

See reflection/describe-base.h for more information about the description API.

#### Resource management

FULL DOCUMENTATION PENDING

The AYU resource tracker allows you to associate files and items in them with
IRIs (unicode URIs), and automatically load those files when requesting the
given IRI.  It also allows files to reference other files using IRIs,
automatically loading those files as well.  It allows live reloading of resource
files, while updating all references that point to the reloaded files.  It also
allows checked unloading, which scans data to prevent dangling references.

Here is some example usage.

```
int main () {
     // Declare a resource scheme
    ayu::FolderResourceScheme res_scheme ("res", cat(here(), "res"));

     // Find an item in a resource file and track the pointer for updates.
     // After this, my_staff will point to a native C++ object that you can do
     // whatever you want with.
    MageStaff* my_staff = ayu::track(staff,
        "res:/liv/mage-equipment.ayu#staff"
    );

     // Reload the resource file.  my_staff will be updated to point to the new
     // value.
    ayu::reload("res:/liv/mage-equipment.ayu");

     // Save the resource file.
    ayu::save("res:/liv/make-equipment.ayu");

     // Try to unload the resource file.  This will throw an exception because
     // my_staff still points to the resource data.
    try {
        ayu::unload("res:/liv/mage-equipment.ayu");
    } catch (Error& e) {
        print_utf8(cat("Failed to unload: ", e.what(), '\n'));
    }

     // This time it will work.
    my_staff = null;
    ayu::unload("res:/liv/mage-equipment.ayu");
}
```

Documentation pending, but see resources/resource.h
