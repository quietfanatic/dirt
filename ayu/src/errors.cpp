#include "../errors.h"

#include "../describe.h"

using namespace ayu;

///// GENERAL

AYU_DESCRIBE(ayu::Error, elems(), attrs())

AYU_DESCRIBE(ayu::GenericError,
    elems(
        elem(base<Error>(), include),
        elem(&GenericError::mess)
    )
)
AYU_DESCRIBE(ayu::IOError,
    elems(
        elem(base<Error>(), include),
        elem(&IOError::filename),
        elem(&IOError::errnum)
    )
)
AYU_DESCRIBE(ayu::OpenFailed,
    elems(
        elem(base<IOError>(), include),
        elem(&OpenFailed::mode)
    )
)
AYU_DESCRIBE(ayu::CloseFailed,
    delegate(base<IOError>())
)

///// document.h

AYU_DESCRIBE(ayu::DocumentError,
    delegate(base<Error>())
)

AYU_DESCRIBE(ayu::DocumentInvalidName,
    elems(
        elem(base<DocumentError>(), include),
        elem(&DocumentInvalidName::name)
    )
)
AYU_DESCRIBE(ayu::DocumentDuplicateName,
    elems(
        elem(base<DocumentError>(), include),
        elem(&DocumentDuplicateName::name)
    )
)
AYU_DESCRIBE(ayu::DocumentDeleteWrongType,
    elems(
        elem(base<DocumentError>(), include),
        elem(&DocumentDeleteWrongType::existing),
        elem(&DocumentDeleteWrongType::deleted_as)
    )
)
AYU_DESCRIBE(ayu::DocumentDeleteMissing,
    elems(
        elem(base<DocumentError>(), include),
        elem(&DocumentDeleteMissing::name)
    )
)

///// location.h

AYU_DESCRIBE(ayu::InvalidLocationIRI,
    elems(
        elem(base<Error>(), include),
        elem(&InvalidLocationIRI::spec),
        elem(&InvalidLocationIRI::mess)
    )
)

///// parse.h

AYU_DESCRIBE(ayu::ParseError,
    attrs(
        attr("Error", base<Error>(), include),
        attr("mess", &ParseError::mess),
        attr("filename", &ParseError::filename),
        attr("line", &ParseError::line),
        attr("col", &ParseError::col)
    )
)

AYU_DESCRIBE(ayu::ReadFailed,
    delegate(base<IOError>())
)

///// print.h

AYU_DESCRIBE(ayu::InvalidPrintOptions,
    elems(
        elem(base<Error>(), include),
        elem(&InvalidPrintOptions::options)
    )
)

AYU_DESCRIBE(ayu::WriteFailed,
    delegate(base<IOError>())
)

///// reference.h

AYU_DESCRIBE(ayu::ReferenceError,
    elems(
        elem(base<Error>(), include),
        elem(&ReferenceError::location),
        elem(&ReferenceError::type)
    )
)
AYU_DESCRIBE(ayu::WriteReadonlyReference,
    elems(elem(base<ReferenceError>(), include))
)
AYU_DESCRIBE(ayu::UnaddressableReference,
    elems(elem(base<ReferenceError>(), include))
)

///// resource-scheme.h

AYU_DESCRIBE(ayu::ResourceNameError,
    delegate(base<Error>())
)
AYU_DESCRIBE(ayu::InvalidResourceName,
    elems(
        elem(base<ResourceNameError>(), include),
        elem(&InvalidResourceName::name)
    )
)
AYU_DESCRIBE(ayu::UnknownResourceScheme,
    elems(
        elem(base<ResourceNameError>(), include),
        elem(&UnknownResourceScheme::name)
    )
)
AYU_DESCRIBE(ayu::UnacceptableResourceName,
    elems(
        elem(base<ResourceNameError>(), include),
        elem(&UnacceptableResourceName::name)
    )
)
AYU_DESCRIBE(ayu::UnacceptableResourceType,
    elems(
        elem(base<ResourceNameError>(), include),
        elem(&UnacceptableResourceType::name),
        elem(&UnacceptableResourceType::type)
    )
)
AYU_DESCRIBE(ayu::InvalidResourceScheme,
    elems(
        elem(base<ResourceNameError>(), include),
        elem(&InvalidResourceScheme::scheme)
    )
)
AYU_DESCRIBE(ayu::DuplicateResourceScheme,
    elems(
        elem(base<ResourceNameError>(), include),
        elem(&DuplicateResourceScheme::scheme)
    )
)

///// resource.h

AYU_DESCRIBE(ayu::ResourceState,
    values(
        value("UNLOADED", UNLOADED),
        value("LOADED", LOADED),
        value("LOAD_CONSTRUCTING", LOAD_CONSTRUCTING),
        value("LOAD_ROLLBACK", LOAD_ROLLBACK),
        value("SAVE_VERIFYING", SAVE_VERIFYING),
        value("SAVE_COMMITTING", SAVE_COMMITTING),
        value("UNLOAD_VERIFYING", UNLOAD_VERIFYING),
        value("UNLOAD_COMMITTING", UNLOAD_COMMITTING),
        value("RELOAD_CONSTRUCTING", RELOAD_CONSTRUCTING),
        value("RELOAD_VERIFYING", RELOAD_VERIFYING),
        value("RELOAD_ROLLBACK", RELOAD_ROLLBACK),
        value("RELOAD_COMMITTING", RELOAD_COMMITTING)
    )
)

AYU_DESCRIBE(ayu::ResourceError,
    delegate(base<Error>())
)
AYU_DESCRIBE(ayu::InvalidResourceState,
    elems(
        elem(base<ResourceError>(), include),
        elem(&InvalidResourceState::tried),
        elem(&InvalidResourceState::state),
        elem(&InvalidResourceState::resource)
    )
)
AYU_DESCRIBE(ayu::EmptyResourceValue,
    elems(
        elem(base<ResourceError>(), include),
        elem(&EmptyResourceValue::name)
    )
)
AYU_DESCRIBE(ayu::UnloadBreak,
    elems(
        elem(&UnloadBreak::from),
        elem(&UnloadBreak::to)
    )
)
AYU_DESCRIBE(ayu::UnloadWouldBreak,
    elems(
        elem(base<ResourceError>(), include),
        elem(&UnloadWouldBreak::breaks)
    )
)
AYU_DESCRIBE(ayu::ReloadBreak,
    elems(
        elem(&ReloadBreak::from),
        elem(&ReloadBreak::to),
        elem(&ReloadBreak::inner)
    )
)
AYU_DESCRIBE(ayu::ReloadWouldBreak,
    elems(
        elem(base<ResourceError>(), include),
        elem(&ReloadWouldBreak::breaks)
    )
)
AYU_DESCRIBE(ayu::RemoveSourceFailed,
    elems(
        elem(base<ResourceError>(), include),
        elem(&RemoveSourceFailed::resource),
        elem(value_func<Str>(
            [](const RemoveSourceFailed& v){
                return Str(std::strerror(v.errnum));
            }
        ))
    )
)

///// scan.h

AYU_DESCRIBE(ayu::ReferenceNotFound,
    elems(
        elem(base<Error>(), include),
        elem(&ReferenceNotFound::type)
    )
)

///// serialize.h

AYU_DESCRIBE(ayu::SerializeFailed,
    attrs(
        attr("Error", base<Error>(), include),
        attr("location", &SerializeFailed::location),
        attr("type", &SerializeFailed::type),
        attr("inner", &SerializeFailed::inner)
    )
)

AYU_DESCRIBE(ayu::ToTreeNotSupported,
    delegate(base<Error>())
)
AYU_DESCRIBE(ayu::FromTreeNotSupported,
    delegate(base<Error>())
)
AYU_DESCRIBE(ayu::InvalidForm,
    elems(
        elem(base<Error>(), include),
        elem(&InvalidForm::form)
    )
)
AYU_DESCRIBE(ayu::NoNameForValue,
    delegate(base<Error>())
)
AYU_DESCRIBE(ayu::NoValueForName,
    elems(
        elem(base<Error>(), include),
        elem(&NoValueForName::name)
    )
)
AYU_DESCRIBE(ayu::MissingAttr,
    elems(
        elem(base<Error>(), include),
        elem(&MissingAttr::key)
    )
)
AYU_DESCRIBE(ayu::UnwantedAttr,
    elems(
        elem(base<Error>(), include),
        elem(&UnwantedAttr::key)
    )
)
AYU_DESCRIBE(ayu::WrongLength,
    attrs(
        attr("ayu::Error", base<Error>(), include),
        attr("min", &WrongLength::min),
        attr("max", &WrongLength::max),
        attr("got", &WrongLength::got)
    )
)
AYU_DESCRIBE(ayu::NoAttrs,
    delegate(base<Error>())
)
AYU_DESCRIBE(ayu::NoElems,
    delegate(base<Error>())
)
AYU_DESCRIBE(ayu::AttrNotFound,
    elems(
        elem(base<Error>(), include),
        elem(&AttrNotFound::key)
    )
)
AYU_DESCRIBE(ayu::ElemNotFound,
    elems(
        elem(base<Error>(), include),
        elem(&ElemNotFound::index)
    )
)
AYU_DESCRIBE(ayu::InvalidKeysType,
    elems(
        elem(base<Error>(), include),
        elem(&InvalidKeysType::keys_type)
    )
)

///// tree.h

AYU_DESCRIBE(ayu::TreeError,
    delegate(base<Error>())
)

AYU_DESCRIBE(ayu::WrongForm,
    elems(
        elem(base<TreeError>(), include),
        elem(&WrongForm::form),
        elem(&WrongForm::tree)
    )
)

AYU_DESCRIBE(ayu::CantRepresent,
    elems(
        elem(base<TreeError>(), include),
        elem(&CantRepresent::type_name),
        elem(&CantRepresent::tree)
    )
)

///// type.h

AYU_DESCRIBE(ayu::TypeError,
    delegate(base<Error>())
)

AYU_DESCRIBE(ayu::UnknownType,
    elems(
        elem(base<TypeError>(), include),
        elem(value_func<UniqueString>(
            [](const ayu::UnknownType& v){ return get_demangled_name(*v.cpp_type); }
        ))
    )
)

AYU_DESCRIBE(ayu::TypeNotFound,
    elems(
        elem(base<TypeError>(), include),
        elem(&TypeNotFound::name)
    )
)

AYU_DESCRIBE(ayu::CannotDefaultConstruct,
    elems(
        elem(base<TypeError>(), include),
        elem(&CannotDefaultConstruct::type)
    )
)
AYU_DESCRIBE(ayu::CannotDestroy,
    elems(
        elem(base<TypeError>(), include),
        elem(&CannotDestroy::type)
    )
)
AYU_DESCRIBE(ayu::CannotCoerce,
    elems(
        elem(base<TypeError>(), include),
        elem(&CannotCoerce::from),
        elem(&CannotCoerce::to)
    )
)
