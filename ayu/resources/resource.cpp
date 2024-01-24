#include "resource.private.h"

#include "../../uni/io.h"
#include "../data/parse.h"
#include "../data/print.h"
#include "../reflection/describe-standard.h"
#include "../reflection/describe.h"
#include "../reflection/dynamic.h"
#include "../reflection/reference.h"
#include "../traversal/from-tree.h"
#include "../traversal/scan.h"
#include "../traversal/to-tree.h"
#include "purpose.h"
#include "scheme.h"
#include "universe.private.h"

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

struct Break {
    SharedLocation from;
    SharedLocation to;
};

[[noreturn, gnu::cold]]
static void raise_would_break (
    ErrorCode code, CopyRef<UniqueArray<Break>> breaks
) {
    UniqueString mess = cat(
        (code == e_ResourceReloadWouldBreak ? "Re" : "Un"),
        "loading resources would break ", breaks->size(), " reference(s): \n"
    );
    for (usize i = 0; i < breaks->size(); ++i) {
        if (i > 5) break;
        mess = cat(move(mess),
            "    ", location_to_iri(breaks[i].from).spec(),
            " -> ", location_to_iri(breaks[i].to).spec(), '\n'
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
    auto a = Slice<Tree>(tree);
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
        case RS::Unloaded: return "RS::Unloaded";
        case RS::Loaded: return "RS::Loaded";
        case RS::LoadConstructing: return "RS::LoadConstructing";
        case RS::LoadReady: return "RS::LoadReady";
        case RS::LoadCancelling: return "RS::LoadCancelling";
        case RS::UnloadVerifying: return "RS::UnloadVerifying";
        case RS::UnloadReady: return "RS::UnloadReady";
        case RS::UnloadCommitting: return "RS::UnloadCommitting";
        case RS::ReloadConstructing: return "RS::ReloadConstructing";
        case RS::ReloadVerifying: return "RS::ReloadVerifying";
        case RS::ReloadReady: return "RS::ReloadReady";
        case RS::ReloadCancelling: return "RS::ReloadCancelling";
        case RS::ReloadCommitting: return "RS::ReloadCommitting";
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
    if (data->state == RS::Unloaded) set_value(move(v));
    else {
        raise_ResourceStateInvalid("construct", *this);
    }
}

const IRI& Resource::name () const noexcept { return data->name; }
ResourceState Resource::state () const noexcept { return data->state; }

Dynamic& Resource::value () const {
    switch (data->state) {
        case RS::LoadCancelling:
        case RS::UnloadReady:
        case RS::UnloadCommitting:
            raise_ResourceStateInvalid("value", *this);
        case RS::Unloaded: load(*this); break;
        default: break;
    }
    return data->value;
}
Dynamic& Resource::get_value () const noexcept {
    return data->value;
}

void Resource::set_value (MoveRef<Dynamic> value) const {
    Dynamic v = *move(value);
    switch (data->state) {
        case RS::Unloaded:
        case RS::LoadReady:
        case RS::Loaded: break;
        default: raise_ResourceStateInvalid("set_value", *this);
    }
    if (!v.has_value()) {
        raise_ResourceValueEmpty("set_value", *this);
    }
    if (data->name) {
        auto scheme = universe().require_scheme(data->name);
        if (!scheme->accepts_type(v.type)) {
            raise_ResourceTypeRejected("set_value", *this, v.type);
        }
    }
    data->value = move(v);
    if (data->state == RS::Unloaded) {
        if (ResourceTransaction::depth) {
            data->state = RS::LoadReady;
            struct SetValueCommitter : Committer {
                Resource res;
                SetValueCommitter (Resource r) : res(r) { }
                void commit () noexcept override {
                    res.data->state = RS::Loaded;
                }
                void rollback () noexcept override {
                    res.data->state = RS::LoadCancelling;
                    res.data->value = {};
                    res.data->state = RS::Unloaded;
                }
            };
            ResourceTransaction::add_committer(new SetValueCommitter(*this));
        }
        else data->state = RS::Loaded;
    }
}

Reference Resource::ref () const {
    return value().ptr();
}
Reference Resource::get_ref () const noexcept {
    return get_value().ptr();
}

///// RESOURCE OPERATIONS

void load (Resource res) {
    current_purpose->acquire(res);
}

static void load_cancel (Resource res) {
    res.data->state = RS::LoadCancelling;
    res.data->value = {};
    res.data->state = RS::Unloaded;
}

void in::load_under_purpose (Resource res) {
    switch (res.data->state) {
        case RS::Loaded:
        case RS::LoadConstructing:
        case RS::LoadReady: return;
        case RS::Unloaded: break;
        default: raise_ResourceStateInvalid("load", res);
    }

    res.data->state = RS::LoadConstructing;
    try {
        auto scheme = universe().require_scheme(res.data->name);
        auto filename = scheme->get_file(res.data->name);
        Tree tree = tree_from_file(move(filename));
        verify_tree_for_scheme(res, scheme, tree);
         // Loading must be under a transaction so that recursive loads are all or
         // nothing.  TODO: Move this into item_from_tree
        ResourceTransaction tr;
         // TODO: Call this on the value, not on the Dynamic.  The location isn't
         // right.
        expect(!res.data->value.has_value());
        item_from_tree(
            &res.data->value, tree, SharedLocation(res),
            FromTreeOptions::DelaySwizzle
        );
    }
    catch (...) { load_cancel(res); throw; }

    if (ResourceTransaction::depth) {
        res.data->state = RS::LoadReady;
        struct LoadCommitter : Committer {
            Resource res;
            LoadCommitter (Resource r) : res(r) { }
            void commit () noexcept override {
                res.data->state = RS::Loaded;
            };
            void rollback () noexcept override {
                load_cancel(res);
            }
        };
        ResourceTransaction::add_committer(
            new LoadCommitter(res)
        );
    }
    else {
        res.data->state = RS::Loaded;
    }
}

void save (Resource res) {
    switch (res.data->state) {
        case RS::Loaded: break;
        default: raise_ResourceStateInvalid("save", res);
    }

    KeepLocationCache klc;
    if (!res.data->value.has_value()) {
        raise_ResourceValueEmpty("save", res);
    }
    auto scheme = universe().require_scheme(res.data->name);
    if (!scheme->accepts_type(res.data->value.type)) {
        raise_ResourceTypeRejected("save", res, res.data->value.type);
    }
    auto filename = scheme->get_file(res.data->name);
    auto contents = tree_to_string(
        item_to_tree(&res.data->value, SharedLocation(res)),
        PrintOptions::Pretty
    );

    if (ResourceTransaction::depth) {
        FILE* file = open_file(filename, "wb");
        struct SaveCommitter : Committer {
            AnyString contents;
            FILE* file;
            AnyString filename;
            SaveCommitter (AnyString&& c, FILE* f, AnyString&& n) :
                contents(move(c)), file(f), filename(move(n))
            { }
            void commit () noexcept override {
                string_to_file(contents, file, filename);
                close_file(file, filename);
            }
        };
        ResourceTransaction::add_committer(
            new SaveCommitter(move(contents), file, move(filename))
        );
    }
    else {
        string_to_file(contents, filename);
    }
}

void unload (Slice<Resource> reses) {
    UniqueArray<Resource> rs;
    for (auto res : reses)
    switch (res.data->state) {
        case RS::Unloaded:
        case RS::UnloadReady: continue;
        case RS::Loaded: rs.push_back(res); break;
        default: raise_ResourceStateInvalid("unload", res);
    }
     // Verify step
    try {
        for (auto res : rs) res.data->state = RS::UnloadVerifying;
        UniqueArray<Resource> others;
        for (auto& [name, other] : universe().resources) {
            switch (other->state) {
                case RS::Unloaded: continue;
                case RS::UnloadVerifying: continue;
                case RS::Loaded: others.emplace_back(&*other); break;
                default: raise_ResourceStateInvalid("scan for unload", &*other);
            }
        }
         // If we're unloading everything, no need to do any scanning.
        if (others) {
             // First build set of references to things being unloaded
            std::unordered_map<Reference, SharedLocation> ref_set;
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
            UniqueArray<Break> breaks;
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
        for (auto res : rs) res.data->state = RS::Loaded;
        throw;
    }
     // Destruct step
    if (ResourceTransaction::depth) {
        for (auto res: rs) res.data->state = RS::UnloadReady;
        struct UnloadCommitter : Committer {
            UniqueArray<Resource> rs;
            UnloadCommitter (UniqueArray<Resource>&& r) :
                rs(move(r))
            { }
            void commit () noexcept override {
                for (auto res: rs) {
                    res.data->value = {};
                    res.data->state = RS::Unloaded;
                }
            }
        };
        ResourceTransaction::add_committer(new UnloadCommitter(move(rs)));
    }
    else {
         // TODO: tail call this
        for (auto res : rs) {
            res.data->value = {};
            res.data->state = RS::Unloaded;
        }
    }
}

void force_unload (Resource res) {
    switch (res.data->state) {
        case RS::Unloaded: return;
        case RS::Loaded: break;
        default: raise_ResourceStateInvalid("force_unload", res);
    }
    if (ResourceTransaction::depth) {
        res.data->state = RS::UnloadReady;
        struct ForceUnloadCommitter : Committer {
            Resource res;
            ForceUnloadCommitter (Resource r) : res(r) { }
            void commit () noexcept override {
                res.data->state = RS::UnloadCommitting;
                res.data->value = {};
                res.data->state = RS::Unloaded;
            }
        };
        ResourceTransaction::add_committer(new ForceUnloadCommitter(res));
    }
    else {
        res.data->state = RS::UnloadCommitting;
        res.data->value = {};
        res.data->state = RS::Unloaded;
    }
}

static void reload_commit (
    Slice<Resource> rs, std::unordered_map<Reference, Reference> updates
) {
    for (auto res : rs) res.data->state = RS::ReloadCommitting;
    for (auto& [ref_ref, new_ref] : updates) {
        if (auto a = ref_ref.address()) {
            reinterpret_cast<Reference&>(*a) = new_ref;
        }
        else ref_ref.write([&new_ref](Mu& v){
            reinterpret_cast<Reference&>(v) = new_ref;
        });
    }
    for (auto res : rs) {
        res.data->value = {};
        res.data->state = RS::Loaded;
    }
}

static void reload_rollback (Slice<Resource> rs) {
    for (auto res : rs) {
        res.data->state = RS::ReloadCancelling;
        res.data->value = move(res.data->old_value);
        res.data->state = RS::Loaded;
    }
}

void reload (Slice<Resource> reses) {
    for (auto res : reses)
    if (res.data->state != RS::Loaded) {
        raise_ResourceStateInvalid("reload", res);
    }
     // Preparation (this won't throw)
    for (auto res : reses) {
        res.data->state = RS::ReloadConstructing;
        expect(!res.data->old_value.has_value());
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
             // Do not DelaySwizzle for reload.  TODO: Forbid reload while a
             // serialization operation is ongoing.
            item_from_tree(&res.data->value, tree, SharedLocation(res));
        }
        for (auto res : reses) {
            res.data->state = RS::ReloadVerifying;
        }
         // Verify step
        UniqueArray<Resource> others;
        for (auto& [name, other] : universe().resources) {
            switch (other->state) {
                case RS::Unloaded: continue;
                case RS::ReloadVerifying: continue;
                case RS::Loaded: others.emplace_back(&*other); break;
                default: raise_ResourceStateInvalid("scan for reload", &*other);
            }
        }
         // If we're reloading everything, no need to do any scanning.
        if (others) {
             // First build mapping of old refs to locationss
            std::unordered_map<Reference, SharedLocation> old_refs;
            for (auto res : reses) {
                scan_references(
                    res.data->old_value.ptr(), SharedLocation(res),
                    [&old_refs](const Reference& ref, LocationRef loc) {
                        old_refs.emplace(ref, loc);
                        return false;
                    }
                );
            }
             // Then build set of ref-refs to update.
            UniqueArray<Break> breaks;
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
    catch (...) { reload_rollback(reses); throw; }
     // Commit step
    if (ResourceTransaction::depth) {
        for (auto res : reses) res.data->state = RS::ReloadReady;
        struct ReloadCommitter : Committer {
            UniqueArray<Resource> rs;
            std::unordered_map<Reference, Reference> updates;
            ReloadCommitter (
                UniqueArray<Resource>&& r, std::unordered_map<Reference, Reference>&& u
            ) : rs(move(r)), updates(move(u))
            { }
            void commit () noexcept override {
                reload_commit(rs, updates);
            }
            void rollback () noexcept override {
                reload_rollback(rs);
            }
        };
        ResourceTransaction::add_committer(
            new ReloadCommitter(reses, move(updates))
        );
    }
    else reload_commit(reses, updates);
}

void rename (Resource old_res, Resource new_res) {
    if (old_res.data->state != RS::Loaded) {
        raise_ResourceStateInvalid("rename from", old_res);
    }
    if (new_res.data->state != RS::Unloaded) {
        raise_ResourceStateInvalid("rename to", new_res);
    }
    new_res.data->value = move(old_res.data->value);
    new_res.data->state = RS::Loaded;
    old_res.data->state = RS::Unloaded;
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
    if (rd->state != RS::Unloaded) {
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
#include "../test/test-environment.private.h"

AYU_DESCRIBE_INSTANTIATE(std::vector<int32*>)

static tap::TestSet tests ("dirt/ayu/resources/resource", []{
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

    is(input.state(), RS::Unloaded, "Resources start out unloaded");
    doesnt_throw([&]{ load(input); }, "load");
    is(input.state(), RS::Loaded, "Resource state is RS::Loaded after loading");
    ok(input.value().has_value(), "Resource has value after loading");

    throws_code<e_ResourceStateInvalid>([&]{
        Resource(input.name(), Dynamic::make<int>(3));
    }, "Creating resource throws on duplicate");

    doesnt_throw([&]{ unload(input); }, "unload");
    is(input.state(), RS::Unloaded, "Resource state is RS::Unloaded after unloading");
    ok(!input.data->value.has_value(), "Resource has no value after unloading");

    ayu::Document* doc = null;
    doesnt_throw([&]{
        doc = &input.value().as<ayu::Document>();
    }, "Getting typed value from a resource");
    is(input.state(), RS::Loaded, "Resource::value() automatically loads resource");
    is(input["foo"][1].get_as<int32>(), 4, "Value was generated properly (0)");
    is(input["bar"][1].get_as<std::string>(), "qux", "Value was generated properly (1)");

    throws_code<e_ResourceStateInvalid>([&]{ save(output); }, "save throws on unloaded resource");

    doc->delete_named("foo");
    doc->new_named<int32>("asdf", 51);

    doesnt_throw([&]{ rename(input, output); }, "rename");
    is(input.state(), RS::Unloaded, "Old resource is RS::Unloaded after renaming");
    is(output.state(), RS::Loaded, "New resource is RS::Loaded after renaming");
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
    SharedLocation loc;
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
    is(Resource(input).state(), RS::Loaded, "Loading second file referencing first file loads first file");
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
