// Provides ayu descriptions for builtin scalar types.  For template types like
// std::vector, include the .h file.

#include "../../iri/iri.h"
#include "../traversal/location.h" // current_base_iri
#include "describe-base.h"

using namespace ayu;

#define AYU_DESCRIBE_SCALAR(type) \
AYU_DESCRIBE(type, \
    to_tree([](const type& v){ return Tree(v); }), \
    from_tree([](type& v, const Tree& t){ v = type(t); }) \
)

AYU_DESCRIBE_SCALAR(std::nullptr_t)
AYU_DESCRIBE_SCALAR(bool)
AYU_DESCRIBE_SCALAR(char)
 // Even though these are in uni::, serialize them without the namespace.
AYU_DESCRIBE_SCALAR(int8)
AYU_DESCRIBE_SCALAR(uint8)
AYU_DESCRIBE_SCALAR(int16)
AYU_DESCRIBE_SCALAR(uint16)
AYU_DESCRIBE_SCALAR(int32)
AYU_DESCRIBE_SCALAR(uint32)
AYU_DESCRIBE_SCALAR(int64)
AYU_DESCRIBE_SCALAR(uint64)
AYU_DESCRIBE_SCALAR(float)
AYU_DESCRIBE_SCALAR(double)
AYU_DESCRIBE_SCALAR(uni::AnyString)
#undef AYU_DESCRIBE_SCALAR
AYU_DESCRIBE(uni::UniqueString,
    to_tree([](const UniqueString& v){ return Tree(AnyString(v)); }),
    from_tree([](UniqueString& v, const Tree& t){ v = AnyString(t); })
)
AYU_DESCRIBE(uni::SharedString,
    to_tree([](const SharedString& v){ return Tree(AnyString(v)); }),
    from_tree([](SharedString& v, const Tree& t){ v = AnyString(t); })
)

 // Str is a reference-like type so it can't be deserialized because the data
 // structure containing it would most likely outlive the tree it came from.
 // However, allowing it to be serialized is useful for error messages.
AYU_DESCRIBE(uni::Str,
    to_tree([](const Str& v){
        return Tree(v);
    })
)
AYU_DESCRIBE(uni::StaticString,
    to_tree([](const StaticString& v){
        return Tree(v);
    })
)
 // We can't do the same for const char* because the full specialization would
 // conflict with the partial specialization for T*.  Normally this wouldn't be
 // a problem, but if the templates are in different compilation units, it'll
 // cause a duplicate definition error from the linker.

AYU_DESCRIBE(iri::IRI,
    to_tree([](const IRI& v){
        return Tree(v.make_relative(current_base_iri()));
    }),
    from_tree([](IRI& v, const Tree& t){
        v = IRI(Str(t), current_base_iri());
    })
)
