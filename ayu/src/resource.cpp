#include "../resource.h"

#include "../../uni/io.h"
#include "../dynamic.h"
#include "../describe.h"
#include "../describe-standard.h"
#include "../parse.h"
#include "../print.h"
#include "../reference.h"
#include "../resource-scheme.h"
#include "../scan.h"
#include "../serialize-from-tree.h"
#include "../serialize-to-tree.h"
#include "universe-private.h"

///// INTERNALS

namespace ayu {
namespace in {

[[noreturn, gnu::cold]]
static void raise_ResourceStateInvalid (StaticString tried, Resource res) {
    raise(e_ResourceStateInvalid, cat(
        "Can't ", tried, ' ', res.name().spec(),
        " when its state is ", show_ResourceState(res.state())
    ));
}

[[noreturn, gnu::cold]]
static void raise_ResourceValueEmpty (StaticString tried, Resource res) {
    raise(e_ResourceValueInvalid, cat(
        "Can't ", tried, ' ', res.name().spec(), " with empty value"
    ));
}

[[noreturn, gnu::cold]]
static void raise_ResourceTypeRejected (
    StaticString tried, Resource res, Type type
) {
    raise(e_ResourceTypeRejected, cat(
        "Can't ", tried, ' ', res.name().spec(), " with type ", type.name()
    ));
}

[[noreturn, gnu::cold]]
static void raise_would_break (
    ErrorCode code, CopyRef<UniqueArray<std::pair<Location, Location>>> breaks
) {
    UniqueString mess = cat(
        (code == e_ResourceReloadWouldBreak ? "Re" : "Un"),
        "loading resources would break ", breaks->size(), " reference(s): \n"
    );
    for (usize i = 0; i < breaks->size(); ++i) {
        if (i > 5) break;
        mess = cat(move(mess),
            "    ", location_to_iri(breaks[i].first).spec(),
            " -> ", location_to_iri(breaks[i].second).spec(), '\n'
        );
    }
    if (breaks->size() > 5) {
        mess = cat(move(mess), "    ...and ", breaks->size() - 5, " others.\n");
    }
    raise(code, move(mess));
}

static void verify_tree_for_scheme (
    Resource res,
    const ResourceScheme* scheme,
    const Tree& tree
) {
    if (tree.form == Form::Null) {
        raise_ResourceValueEmpty("load", res);
    }
    auto a = TreeArraySlice(tree);
    if (a.size() == 2) {
        Type type = Type(Str(a[0]));
        if (!scheme->accepts_type(type)) {
            raise_ResourceTypeRejected("load", res, type);
        }
    }
    else {
         // TODO: throw ResourceValueInvalid
        require(false);
    }
}

} using namespace in;

StaticString show_ResourceState (ResourceState state) noexcept {
    switch (state) {
        case UNLOADED: return "UNLOADED";
        case LOADED: return "LOADED";
        case LOAD_CONSTRUCTING: return "LOAD_CONSTRUCTING";
        case LOAD_ROLLBACK: return "LOAD_ROLLBACK";
        case SAVE_VERIFYING: return "SAVE_VERIFYING";
        case SAVE_COMMITTING: return "SAVE_COMMITTING";
        case UNLOAD_VERIFYING: return "UNLOAD_VERIFYING";
        case UNLOAD_COMMITTING: return "UNLOAD_COMMITTING";
        case RELOAD_CONSTRUCTING: return "RELOAD_CONSTRUCTING";
        case RELOAD_VERIFYING: return "RELOAD_VERIFYING";
        case RELOAD_ROLLBACK: return "RELOAD_ROLLBACK";
        case RELOAD_COMMITTING: return "RELOAD_COMMITTING";
        default: never();
    }
}

///// RESOURCES

Resource::Resource (const IRI& name) {
    if (!name || name.has_fragment()) {
        raise(e_ResourceNameInvalid, name.possibly_invalid_spec());
    }
    auto scheme = universe().require_scheme(name);
    if (!scheme->accepts_iri(name)) {
        raise(e_ResourceNameRejected, name.spec());
    }
    auto& resources = universe().resources;
    auto iter = resources.find(name.spec());
    if (iter != resources.end()) {
        data = &*iter->second;
    }
    else {
        auto ptr = std::make_unique<ResourceData>(name);
        data = &*ptr;
         // Be careful about storing the right Str
        resources.emplace(data->name.spec(), move(ptr));
    }
}
Resource::Resource (Str ref) :
    Resource(IRI(ref, current_base_iri()))
{ }

Resource::Resource (const IRI& name, MoveRef<Dynamic> value) :
    Resource(name)
{
    Dynamic v = *move(value);
    if (!v.has_value()) {
        raise_ResourceValueEmpty("construct", *this);
    }
    if (data->state == UNLOADED) set_value(move(v));
    else {
        raise_ResourceStateInvalid("construct", *this);
    }
}

const IRI& Resource::name () const noexcept { return data->name; }
ResourceState Resource::state () const noexcept { return data->state; }

Dynamic& Resource::value () const {
    if (data->state == UNLOADED) {
        load(*this);
    }
    return data->value;
}
Dynamic& Resource::get_value () const noexcept {
    return data->value;
}
void Resource::set_value (MoveRef<Dynamic> value) const {
    Dynamic v = *move(value);
    if (!v.has_value()) {
        raise_ResourceValueEmpty("set_value", *this);
    }
    if (data->name) {
        auto scheme = universe().require_scheme(data->name);
        if (!scheme->accepts_type(v.type)) {
            raise_ResourceTypeRejected("set_value", *this, v.type);
        }
    }
    switch (data->state) {
        case UNLOADED:
            data->state = LOADED;
            break;
        case LOAD_CONSTRUCTING:
        case LOADED:
            break;
        default: raise_ResourceStateInvalid("set_value", *this);
    }
    data->value = move(v);
}

Reference Resource::ref () const {
    return value().ptr();
}
Reference Resource::get_ref () const noexcept {
    if (data->state == UNLOADED) return Reference();
    else return get_value().ptr();
}

///// RESOURCE OPERATIONS

void load (Resource res) {
    load(Slice<Resource>(&res, 1));
}
void load (Slice<Resource> reses) {
    UniqueArray<Resource> rs;
    for (auto res : reses)
    switch (res.data->state) {
        case UNLOADED: rs.push_back(res); break;
        case LOADED:
        case LOAD_CONSTRUCTING: continue;
        default: raise_ResourceStateInvalid("load", res);
    }
    try {
        for (auto res : rs) res.data->state = LOAD_CONSTRUCTING;
        for (auto res : rs) {
            auto scheme = universe().require_scheme(res.data->name);
            auto filename = scheme->get_file(res.data->name);
            Tree tree = tree_from_file(move(filename));
            verify_tree_for_scheme(res, scheme, tree);
            item_from_tree(
                &res.data->value, tree, Location(res), DELAY_SWIZZLE
            );
        }
        for (auto res : rs) res.data->state = LOADED;
    }
    catch (...) {
         // TODO: When recursing load(), rollback innerly loading resources if
         // an outerly loading resource throws.
        for (auto res : rs) res.data->state = LOAD_ROLLBACK;
        for (auto res : rs) {
            try {
                res.data->value = Dynamic();
            }
            catch (...) {
                unrecoverable_exception("while rolling back load");
            }
            res.data->state = UNLOADED;
        }
        throw;
    }
}

void rename (Resource old_res, Resource new_res) {
    if (old_res.data->state != LOADED) {
        raise_ResourceStateInvalid("rename from", old_res);
    }
    if (new_res.data->state != UNLOADED) {
        raise_ResourceStateInvalid("rename to", new_res);
    }
    new_res.data->value = move(old_res.data->value);
    new_res.data->state = LOADED;
    old_res.data->state = UNLOADED;
}

void save (Resource res) {
    save(Slice<Resource>(&res, 1));
}
void save (Slice<Resource> reses) {
    for (auto res : reses) {
        if (res.data->state != LOADED) raise_ResourceStateInvalid("save", res);
    }
    try {
        for (auto res : reses) res.data->state = SAVE_VERIFYING;
         // Serialize all before writing to disk
        UniqueArray<std::function<void()>> committers (reses.size());
        {
            KeepLocationCache klc;
            for (usize i = 0; i < reses.size(); i++) {
                Resource res = reses[i];
                if (!res.data->value.has_value()) {
                    raise_ResourceValueEmpty("save", res);
                }
                auto scheme = universe().require_scheme(res.data->name);
                if (!scheme->accepts_type(res.data->value.type)) {
                    raise_ResourceTypeRejected("save", res, res.data->value.type);
                }
                auto filename = scheme->get_file(res.data->name);
                auto contents = tree_to_string(
                    item_to_tree(&res.data->value, Location(res))
                );
                committers[i] = [
                    contents{move(contents)},
                    filename{move(filename)}
                ]{
                    string_to_file(contents, move(filename));
                };
            }
        }
        for (auto res : reses) res.data->state = SAVE_COMMITTING;
        for (auto& commit : committers) commit();
        for (auto res : reses) res.data->state = LOADED;
    }
    catch (...) {
        for (auto res : reses) res.data->state = LOADED;
        throw;
    }
}

void unload (Resource res) {
    unload(Slice<Resource>(&res, 1));
}
void unload (Slice<Resource> reses) {
    UniqueArray<Resource> rs;
    for (auto res : reses)
    switch (res.data->state) {
        case UNLOADED: continue;
        case LOADED: rs.push_back(res); break;
        default: raise_ResourceStateInvalid("unload", res);
    }
     // Verify step
    try {
        for (auto res : rs) res.data->state = UNLOAD_VERIFYING;
        UniqueArray<Resource> others;
        for (auto& [name, other] : universe().resources) {
            switch (other->state) {
                case UNLOADED: continue;
                case UNLOAD_VERIFYING: continue;
                case LOADED: others.emplace_back(&*other); break;
                default: raise_ResourceStateInvalid("scan for unload", &*other);
            }
        }
         // If we're unloading everything, no need to do any scanning.
        if (others) {
             // First build set of references to things being unloaded
            std::unordered_map<Reference, Location> ref_set;
            for (auto res : rs) {
                scan_resource_references(
                    res,
                    [&ref_set](const Reference& ref, LocationRef loc) {
                        ref_set.emplace(ref, loc);
                        return false;
                    }
                );
            }
             // Then check if any other resources contain references in that set
            UniqueArray<std::pair<Location, Location>> breaks;
            for (auto other : others) {
                scan_resource_references(
                    other,
                    [&ref_set, &breaks](Reference ref_ref, LocationRef loc) {
                         // TODO: Check for Pointer as well
                        if (ref_ref.type() != Type::CppType<Reference>()) {
                            return false;
                        }
                        Reference ref = ref_ref.get_as<Reference>();
                        auto iter = ref_set.find(ref);
                        if (iter != ref_set.end()) {
                            breaks.emplace_back(loc, iter->second);
                        }
                        return false;
                    }
                );
            }
            if (breaks) {
                raise_would_break(e_ResourceUnloadWouldBreak, move(breaks));
            }
        }
    }
    catch (...) {
        for (auto res : rs) res.data->state = LOADED;
        throw;
    }
     // Destruct step
    for (auto res : rs) res.data->state = UNLOAD_COMMITTING;
    try {
        for (auto res : rs) {
            res.data->value = Dynamic();
            res.data->state = UNLOADED;
        }
    }
    catch (...) {
        unrecoverable_exception("while running destructor during unload");
    }
}

void force_unload (Resource res) {
    force_unload(Slice<Resource>(&res, 1));
}
void force_unload (Slice<Resource> reses) {
    UniqueArray<Resource> rs;
    for (auto res : reses)
    switch (res.data->state) {
        case UNLOADED: continue;
        case LOADED: rs.push_back(res); break;
        default: raise_ResourceStateInvalid("force_unload", res);
    }
     // Skip straight to destruct step
    for (auto res : rs) res.data->state = UNLOAD_COMMITTING;
    try {
        for (auto res : rs) {
            res.data->value = Dynamic();
            res.data->state = UNLOADED;
        }
    }
    catch (...) {
        unrecoverable_exception("while running destructor during force_unload");
    }
}

void reload (Resource res) {
    reload(Slice<Resource>(&res, 1));
}
void reload (Slice<Resource> reses) {
    for (auto res : reses)
    if (res.data->state != LOADED) {
        raise_ResourceStateInvalid("reload", res);
    }
     // Preparation (this won't throw)
    for (auto res : reses) {
        res.data->state = RELOAD_CONSTRUCTING;
        res.data->old_value = move(res.data->value);
    }
    std::unordered_map<Reference, Reference> updates;
    try {
         // Construct step
        for (auto res : reses) {
            auto scheme = universe().require_scheme(res.data->name);
            auto filename = scheme->get_file(res.data->name);
            Tree tree = tree_from_file(move(filename));
            verify_tree_for_scheme(res, scheme, tree);
             // Do not DELAY_SWIZZLE for reload.  TODO: Forbid reload while a
             // serialization operation is ongoing.
            item_from_tree(&res.data->value, tree, Location(res));
        }
        for (auto res : reses) {
            res.data->state = RELOAD_VERIFYING;
        }
         // Verify step
        UniqueArray<Resource> others;
        for (auto& [name, other] : universe().resources) {
            switch (other->state) {
                case UNLOADED: continue;
                case RELOAD_VERIFYING: continue;
                case LOADED: others.emplace_back(&*other); break;
                default: raise_ResourceStateInvalid("scan for reload", &*other);
            }
        }
         // If we're reloading everything, no need to do any scanning.
        if (others) {
             // First build mapping of old refs to locationss
            std::unordered_map<Reference, Location> old_refs;
            for (auto res : reses) {
                scan_references(
                    res.data->old_value.ptr(), Location(res),
                    [&old_refs](const Reference& ref, LocationRef loc) {
                        old_refs.emplace(ref, loc);
                        return false;
                    }
                );
            }
             // Then build set of ref-refs to update.
            UniqueArray<std::pair<Location, Location>> breaks;
            for (auto other : others) {
                scan_resource_references(
                    other,
                    [&updates, &old_refs, &breaks](Reference ref_ref, LocationRef loc) {
                         // TODO: scan Pointers as well
                        if (ref_ref.type() != Type::CppType<Reference>()) return false;
                        Reference ref = ref_ref.get_as<Reference>();
                        auto iter = old_refs.find(ref);
                        if (iter == old_refs.end()) return false;
                        try {
                             // reference_from_location will use new resource value
                            Reference new_ref = reference_from_location(iter->second);
                            updates.emplace(ref_ref, new_ref);
                        }
                        catch (std::exception&) {
                             breaks.emplace_back(loc, iter->second);
                        }
                        return false;
                    }
                );
            }
            if (breaks) {
                raise_would_break(e_ResourceReloadWouldBreak, move(breaks));
            }
        }
    }
    catch (...) {
        for (auto res : reses) res.data->state = RELOAD_ROLLBACK;
        for (auto res : reses) {
            try {
                res.data->value = Dynamic();
            }
            catch (...) {
                unrecoverable_exception("while rolling back reload");
            }
            res.data->value = move(res.data->old_value);
        }
        for (auto res : reses) res.data->state = LOADED;
        throw;
    }
     // Commit step
    try {
        for (auto& [ref_ref, new_ref] : updates) {
            if (auto a = ref_ref.address()) {
                reinterpret_cast<Reference&>(*a) = new_ref;
            }
            else ref_ref.write([&new_ref](Mu& v){
                reinterpret_cast<Reference&>(v) = new_ref;
            });
        }
    }
    catch (...) {
        unrecoverable_exception("while updating references for reload");
    }
     // Destruct step
    for (auto res : reses) res.data->state = RELOAD_COMMITTING;
    try {
        for (auto res : reses) res.data->value = Dynamic();
    }
    catch (...) {
        unrecoverable_exception("while destructing old values for reload");
    }
    for (auto res : reses) res.data->state = LOADED;
}

AnyString resource_filename (Resource res) {
    auto scheme = universe().require_scheme(res.data->name);
    return scheme->get_file(res.data->name);
}

void remove_source (Resource res) {
    auto scheme = universe().require_scheme(res.data->name);
    auto filename = scheme->get_file(res.data->name);
    remove_utf8(filename.c_str());
}

bool source_exists (Resource res) {
    auto scheme = universe().require_scheme(res.data->name);
    auto filename = scheme->get_file(res.data->name);
    if (std::FILE* f = fopen_utf8(filename.c_str())) {
        fclose(f);
        return true;
    }
    else return false;
}

UniqueArray<Resource> loaded_resources () noexcept {
    UniqueArray<Resource> r;
    for (auto& [name, rd] : universe().resources)
    if (rd->state != UNLOADED) {
        r.push_back(&*rd);
    }
    return r;
}

} using namespace ayu;

///// DESCRIPTIONS

AYU_DESCRIBE(ayu::Resource,
    delegate(const_ref_funcs<IRI>(
        [](const Resource& v) -> const IRI& {
            return v.data->name;
        },
        [](Resource& v, const IRI& m){
            v = Resource(m);
        }
    ))
)

///// TESTS

#ifndef TAP_DISABLE_TESTS
#include "test-environment-private.h"

AYU_DESCRIBE_INSTANTIATE(std::vector<int32*>)

static tap::TestSet tests ("dirt/ayu/resource", []{
    using namespace tap;

    test::TestEnvironment env;

    Resource input ("ayu-test:/testfile.ayu");
    Resource input2 ("ayu-test:/othertest.ayu");
    Resource rec1 ("ayu-test:/rec1.ayu");
    Resource rec2 ("ayu-test:/rec2.ayu");
    Resource badinput ("ayu-test:/badref.ayu");
    Resource output ("ayu-test:/test-output.ayu");
    Resource unicode ("ayu-test:/ユニコード.ayu");
    Resource unicode2 ("ayu-test:/ユニコード2.ayu");

    is(input.state(), UNLOADED, "Resources start out unloaded");
    doesnt_throw([&]{ load(input); }, "load");
    is(input.state(), LOADED, "Resource state is LOADED after loading");
    ok(input.value().has_value(), "Resource has value after loading");

    throws_code<e_ResourceStateInvalid>([&]{
        Resource(input.name(), Dynamic(3));
    }, "Creating resource throws on duplicate");

    doesnt_throw([&]{ unload(input); }, "unload");
    is(input.state(), UNLOADED, "Resource state is UNLOADED after unloading");
    ok(!input.data->value.has_value(), "Resource has no value after unloading");

    ayu::Document* doc = null;
    doesnt_throw([&]{
        doc = &input.value().as<ayu::Document>();
    }, "Getting typed value from a resource");
    is(input.state(), LOADED, "Resource::value() automatically loads resource");
    is(input["foo"][1].get_as<int32>(), 4, "Value was generated properly (0)");
    is(input["bar"][1].get_as<std::string>(), "qux", "Value was generated properly (1)");

    throws_code<e_ResourceStateInvalid>([&]{ save(output); }, "save throws on unloaded resource");

    doc->delete_named("foo");
    doc->new_named<int32>("asdf", 51);

    doesnt_throw([&]{ rename(input, output); }, "rename");
    is(input.state(), UNLOADED, "Old resource is UNLOADED after renaming");
    is(output.state(), LOADED, "New resource is LOADED after renaming");
    is(&output.value().as<ayu::Document>(), doc, "Rename moves value without reconstructing it");

    doesnt_throw([&]{ save(output); }, "save");
    is(tree_from_file(resource_filename(output.name())), tree_from_string(
        "[ayu::Document {bar:[std::string qux] asdf:[int32 51] _next_id:0}]"
    ), "Resource was saved with correct contents");
    ok(source_exists(output), "source_exists returns true before deletion");
    doesnt_throw([&]{ remove_source(output); }, "remove_source");
    ok(!source_exists(output), "source_exists returns false after deletion");
    throws_code<e_OpenFailed>([&]{
        tree_from_file(resource_filename(output.name()));
    }, "Can't open file after calling remove_source");
    doesnt_throw([&]{ remove_source(output); }, "Can call remove_source twice");
    Location loc;
    doesnt_throw([&]{
        item_from_string(&loc, cat('"', input.name().spec(), "#/bar+1\""));
    }, "Can read location from tree");
    Reference ref;
    doesnt_throw([&]{
        ref = reference_from_location(loc);
    }, "reference_from_location");
    doesnt_throw([&]{
        is(ref.get_as<std::string>(), "qux", "reference_from_location got correct item");
    });
    doc = &output.value().as<ayu::Document>();
    ref = output["asdf"][1].address_as<int32>();
    doesnt_throw([&]{
        loc = reference_to_location(ref);
    });
    is(item_to_tree(&loc), tree_from_string("\"ayu-test:/test-output.ayu#/asdf+1\""), "reference_to_location works");
    doc->new_<Reference>(output["bar"][1]);
    doesnt_throw([&]{ save(output); }, "save with reference");
    doc->new_<int32*>(output["asdf"][1]);
    doesnt_throw([&]{ save(output); }, "save with pointer");
    is(tree_from_file(resource_filename(output.name())), tree_from_string(
        "[ayu::Document {bar:[std::string qux] asdf:[int32 51] _0:[ayu::Reference #/bar+1] _1:[int32* #/asdf+1] _next_id:2}]"
    ), "File was saved with correct reference as location");
    throws_code<e_OpenFailed>([&]{
        load(badinput);
    }, "Can't load file with incorrect reference in it");

    doesnt_throw([&]{
        unload(input);
        load(input2);
    }, "Can load second file referencing first");
    is(Resource(input).state(), LOADED, "Loading second file referencing first file loads first file");
    std::string* bar;
    doesnt_throw([&]{
        bar = input["bar"][1];
    }, "can use [] syntax on resources and references");
    is(
        input2["ext_pointer"][1].get_as<std::string*>(),
        bar,
        "Loading a pointer worked!"
    );

    int asdf = 0;
    doesnt_throw([&]{
        asdf = *unicode["ptr"][1].get_as<int*>();
    }, "Can load and reference files with unicode in their name");
    is(asdf, 4444);

    is(
        unicode2["self_pointer"][1].get_as<std::string*>(),
        unicode2["val"][1].address_as<std::string>(),
        "Loading pointer with \"#\" for own file worked."
    );
    throws_code<e_ResourceUnloadWouldBreak>([&]{
        unload(input);
    }, "Can't unload resource when there are references to it");
    doesnt_throw([&]{
        unload(input2);
        unload(input);
    }, "Can unload if we unload the referring resource first");
    doesnt_throw([&]{
        load(rec1);
    }, "Can load resources with reference cycle");
    throws_code<e_ResourceUnloadWouldBreak>([&]{
        unload(rec1);
    }, "Can't unload part of a reference cycle 1");
    throws_code<e_ResourceUnloadWouldBreak>([&]{
        unload(rec2);
    }, "Can't unload part of a reference cycle 2");
    doesnt_throw([&]{
        unload({rec1, rec2});
    }, "Can unload reference cycle by unload both resources at once");
    load(rec1);
    int* old_p = rec1["ref"][1].get_as<int*>();
    doesnt_throw([&]{
        reload(rec2);
    }, "Can reload file with references to it");
    isnt(rec1["ref"][1].get_as<int*>(), old_p, "Reference to reloaded file was updated");

    throws_code<e_ResourceTypeRejected>([&]{
        load("ayu-test:/wrongtype.ayu");
    }, "ResourceScheme::accepts_type rejects wrong type");

    done_testing();
});
#endif
