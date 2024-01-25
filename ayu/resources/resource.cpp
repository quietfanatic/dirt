#include "resource.private.h"

#include "../../uni/io.h"
#include "../data/parse.h"
#include "../data/print.h"
#include "../reflection/describe-standard.h"
#include "../reflection/describe.h"
#include "../reflection/dynamic.h"
#include "../reflection/reference.h"
#include "../traversal/compound.h"
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
static void raise_ResourceStateInvalid (StaticString tried, ResourceRef res) {
    raise(e_ResourceStateInvalid, cat(
        "Can't ", tried, ' ', res->name().spec(),
        " when its state is ", show_ResourceState(res->state())
    ));
}

[[noreturn, gnu::cold]]
static void raise_ResourceValueEmpty (StaticString tried, ResourceRef res) {
    raise(e_ResourceValueInvalid, cat(
        "Can't ", tried, ' ', res->name().spec(), " with empty value"
    ));
}

[[noreturn, gnu::cold]]
static void raise_ResourceTypeRejected (
    StaticString tried, ResourceRef res, Type type
) {
    raise(e_ResourceTypeRejected, cat(
        "Can't ", tried, ' ', res->name().spec(), " with type ", type.name()
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

struct TypeAndTree {
    Type type;
    Tree tree;
};

static TypeAndTree verify_tree_for_scheme (
    ResourceRef res,
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
        return {type, a[1]};
    }
    else raise_LengthRejected(Type::CppType<Dynamic>(), 2, 2, a.size());
}

template <class T>
static void set_states (const T& rs, ResourceState state) {
    for (auto r : rs) {
        static_cast<ResourceData&>(*r).state = state;
    }
}

} using namespace in;

StaticString show_ResourceState (ResourceState state) noexcept {
    switch (state) {
        case RS::Unloaded: return "Unloaded";
        case RS::Loaded: return "Loaded";
        case RS::LoadConstructing: return "LoadConstructing";
        case RS::LoadReady: return "LoadReady";
        case RS::LoadCancelling: return "LoadCancelling";
        case RS::UnloadVerifying: return "UnloadVerifying";
        case RS::UnloadReady: return "UnloadReady";
        case RS::UnloadCommitting: return "UnloadCommitting";
        case RS::ReloadConstructing: return "ReloadConstructing";
        case RS::ReloadVerifying: return "ReloadVerifying";
        case RS::ReloadReady: return "ReloadReady";
        case RS::ReloadCancelling: return "ReloadCancelling";
        case RS::ReloadCommitting: return "ReloadCommitting";
        default: never();
    }
}

///// ACCESSORS

const IRI& Resource::name () const noexcept {
    return static_cast<const ResourceData*>(this)->name;
}
ResourceState Resource::state () const noexcept {
    return static_cast<const ResourceData*>(this)->state;
}

Dynamic& Resource::value () {
    auto data = static_cast<ResourceData*>(this);
    switch (data->state) {
        case RS::LoadCancelling:
        case RS::UnloadReady:
        case RS::UnloadCommitting:
            raise_ResourceStateInvalid("value", ResourceRef(this));
        case RS::Unloaded: load(ResourceRef(this)); break;
        default: break;
    }
    return data->value;
}
Dynamic& Resource::get_value () noexcept {
    return static_cast<ResourceData*>(this)->value;
}

void Resource::set_value (MoveRef<Dynamic> value) {
    auto data = static_cast<ResourceData*>(this);
    Dynamic v = *move(value);
    switch (data->state) {
        case RS::Unloaded:
        case RS::Loaded: break;
        default: raise_ResourceStateInvalid("set_value", this);
    }
    if (!v.has_value()) {
        raise_ResourceValueEmpty("set_value", this);
    }
    if (data->name) {
        auto scheme = universe().require_scheme(data->name);
        if (!scheme->accepts_type(v.type)) {
            raise_ResourceTypeRejected("set_value", this, v.type);
        }
    }
    data->value = move(v);
    if (data->state == RS::Unloaded) {
        if (ResourceTransaction::depth) {
            data->state = RS::LoadReady;
            struct SetValueCommitter : Committer {
                SharedResource res;
                SetValueCommitter (SharedResource&& r) : res(move(r)) { }
                void commit () noexcept override {
                    auto data = static_cast<ResourceData*>(res.data.p);
                    data->state = RS::Loaded;
                }
                void rollback () noexcept override {
                    auto data = static_cast<ResourceData*>(res.data.p);
                    data->state = RS::LoadCancelling;
                    data->value = {};
                    data->state = RS::Unloaded;
                }
            };
            ResourceTransaction::add_committer(new SetValueCommitter(this));
        }
        else data->state = RS::Loaded;
    }
}

///// CONSTRUCTION, DESTRUCTION

SharedResource::SharedResource (const IRI& name) {
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
        data = iter->second.data;
    }
    else {
        expect(!data);
        data = new ResourceData(name);
         // Be careful about storing the right Str.  Well okay, technically it
         // doesn't matter since the strings are refcounted and share the same
         // buffer, but
        resources.emplace(data->name().spec(), *this);
    }
}
SharedResource::SharedResource (Str ref) :
    SharedResource(IRI(ref, current_base_iri()))
{ }

SharedResource::SharedResource (const IRI& name, MoveRef<Dynamic> value) :
    SharedResource(name)
{
    Dynamic v = *move(value);
    if (!v.has_value()) {
        raise_ResourceValueEmpty("construct", *this);
    }
    else if (data->state() == RS::Unloaded) data->set_value(move(v));
    else raise_ResourceStateInvalid("construct", *this);
}

void in::delete_Resource (Resource* res) noexcept {
    auto data = static_cast<ResourceData*>(res);
    universe().resources.erase(data->name.spec());
    delete data;
}

///// RESOURCE OPERATIONS

void load (ResourceRef res) {
    current_purpose->acquire(res);
}

static void load_cancel (ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    data->state = RS::LoadCancelling;
    data->value = {};
    data->state = RS::Unloaded;
}

void in::load_under_purpose (ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    switch (data->state) {
        case RS::Loaded:
        case RS::LoadConstructing:
        case RS::LoadReady: return;
        case RS::Unloaded: break;
        default: raise_ResourceStateInvalid("load", res);
    }

    data->state = RS::LoadConstructing;
    try {
        auto scheme = universe().require_scheme(data->name);
        auto filename = scheme->get_file(data->name);
        Tree tree = tree_from_file(move(filename));
        auto tnt = verify_tree_for_scheme(res, scheme, tree);
        expect(!data->value.has_value());
         // Run item_from_tree on the Dynamic's value, not on the Dynamic
         // itself.  Otherwise, the associated locations will have an extra +1
         // in the fragment.
        data->value = Dynamic(tnt.type);
        item_from_tree(
            data->value.ptr(), tnt.tree, SharedLocation(res),
            FromTreeOptions::DelaySwizzle
        );
    }
    catch (...) { load_cancel(res); throw; }

    if (ResourceTransaction::depth) {
        data->state = RS::LoadReady;
        struct LoadCommitter : Committer {
            SharedResource res;
            LoadCommitter (SharedResource&& r) : res(move(r)) { }
            void commit () noexcept override {
                static_cast<ResourceData&>(*res).state = RS::Loaded;
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
        data->state = RS::Loaded;
    }
}

void save (ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    switch (data->state) {
        case RS::Loaded: break;
        default: raise_ResourceStateInvalid("save", res);
    }

    KeepLocationCache klc;
    if (!data->value.has_value()) {
        raise_ResourceValueEmpty("save", res);
    }
    auto scheme = universe().require_scheme(data->name);
    if (!scheme->accepts_type(data->value.type)) {
        raise_ResourceTypeRejected("save", res, data->value.type);
    }
    auto filename = scheme->get_file(data->name);
     // Do type and value separately, because the Location refers to the value,
     // not the whole Dynamic.
    auto type_tree = item_to_tree(&data->value.type);
    auto value_tree = item_to_tree(data->value.ptr(), SharedLocation(res));
    auto contents = tree_to_string(
        Tree::array(move(type_tree), move(value_tree)),
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

NOINLINE static void unload_commit (MoveRef<UniqueArray<SharedResource>> rs) {
    auto reses = *move(rs);
    reses.consume([](SharedResource&& res){
        auto data = static_cast<ResourceData*>(res.data.p);
        data->value = {};
        data->state = RS::Unloaded;
    });
}

void unload (Slice<ResourceRef> reses) {
    UniqueArray<SharedResource> rs;
    for (auto res : reses)
    switch (res->state()) {
        case RS::Unloaded:
        case RS::UnloadReady: continue;
        case RS::Loaded: rs.push_back(res); break;
        default: raise_ResourceStateInvalid("unload", res);
    }
     // Verify step
    try {
        set_states(rs, RS::UnloadVerifying);
        UniqueArray<SharedResource> others;
        for (auto& [name, other] : universe().resources) {
            switch (other->state()) {
                case RS::Unloaded: continue;
                case RS::UnloadVerifying: continue;
                case RS::Loaded: others.emplace_back(other); break;
                default: raise_ResourceStateInvalid("scan for unload", other);
            }
        }
         // If we're unloading everything, no need to do any scanning.
        if (others) {
             // First build set of references to things being unloaded
            std::unordered_map<Reference, SharedLocation> ref_set;
            for (ResourceRef res : rs) {
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
            for (ResourceRef other : others) {
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
        set_states(rs, RS::Loaded);
        throw;
    }
     // Destruct step
    if (ResourceTransaction::depth) {
        set_states(rs, RS::UnloadReady);
        struct UnloadCommitter : Committer {
            UniqueArray<SharedResource> rs;
            UnloadCommitter (UniqueArray<SharedResource>&& r) :
                rs(move(r))
            { }
            void commit () noexcept override {
                unload_commit(move(rs));
            }
        };
        ResourceTransaction::add_committer(new UnloadCommitter(move(rs)));
    }
    else unload_commit(move(rs));
}

NOINLINE static void force_unload_commit (ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    data->state = RS::UnloadCommitting;
    data->value = {};
    data->state = RS::Unloaded;
}

void force_unload (ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    switch (data->state) {
        case RS::Unloaded: return;
        case RS::Loaded: break;
        default: raise_ResourceStateInvalid("force_unload", res);
    }
    if (ResourceTransaction::depth) {
        data->state = RS::UnloadReady;
        struct ForceUnloadCommitter : Committer {
            SharedResource res;
            ForceUnloadCommitter (SharedResource&& r) : res(move(r)) { }
            void commit () noexcept override { force_unload_commit(res); }
        };
        ResourceTransaction::add_committer(new ForceUnloadCommitter(res));
    }
    else force_unload_commit(res);
}

struct Update {
    Reference ref_ref;
    Reference new_ref;
};

NOINLINE static void reload_commit (
    Slice<ResourceRef> rs, MoveRef<UniqueArray<Update>> ups
) {
    set_states(rs, RS::ReloadCommitting);
    auto updates = *move(ups);
    updates.consume([](Update&& update){
        if (auto a = update.ref_ref.address()) {
            reinterpret_cast<Reference&>(*a) = move(update.new_ref);
        }
        else update.ref_ref.write([new_ref{&update.new_ref} ](Mu& v){
            reinterpret_cast<Reference&>(v) = move(*new_ref);
        });
        expect(!update.new_ref);
    });
    set_states(rs, RS::Loaded);
}

NOINLINE static void reload_rollback (
    Slice<ResourceRef> rs, MoveRef<UniqueArray<Dynamic>> olds
) {
    auto old_values = *move(olds);
    auto begin = old_values.begin();
    old_values.consume_reverse([rs, begin](Dynamic&& old){
        auto data = static_cast<ResourceData*>(rs[&old - begin].data);
        data->state = RS::ReloadCancelling;
        data->value = move(old);
        data->state = RS::Loaded;
    });
}

void reload (Slice<ResourceRef> reses) {
    for (auto res : reses) {
        if (res->state() != RS::Loaded) {
            raise_ResourceStateInvalid("reload", res);
        }
    }
     // Preparation (this won't throw)
    auto old_values = UniqueArray<Dynamic>(
        reses.size(), [reses](usize i) -> Dynamic&&
    {
        auto data = static_cast<ResourceData*>(reses[i].data);
        data->state = RS::ReloadConstructing;
        return move(data->value);
    });

     // Keep track of what references to update to what
    UniqueArray<Update> updates;
    try {
         // Construct step
        for (auto res : reses) {
            auto data = static_cast<ResourceData*>(res.data);
            auto scheme = universe().require_scheme(data->name);
            auto filename = scheme->get_file(data->name);
            Tree tree = tree_from_file(move(filename));
            auto tnt = verify_tree_for_scheme(res, scheme, tree);
            expect(!data->value.has_value());
            data->value = Dynamic(tnt.type);
             // Do not DelaySwizzle for reload.  TODO: Forbid reload while a
             // serialization operation is ongoing.
            item_from_tree(data->value.ptr(), tnt.tree, SharedLocation(res));
        }
        set_states(reses, RS::ReloadVerifying);
         // Verify step
        UniqueArray<ResourceRef> others;
        for (auto& [name, other] : universe().resources) {
            switch (other->state()) {
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
            for (usize i = 0; i < reses.size(); i++) {
                scan_references(
                    old_values[i].ptr(), SharedLocation(reses[i]),
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
                         // TODO: check for Pointer as well?
                        if (ref_ref.type() != Type::CppType<Reference>()) return false;
                        Reference ref = ref_ref.get_as<Reference>();
                        auto iter = old_refs.find(ref);
                        if (iter == old_refs.end()) return false;
                        try {
                            Reference new_ref = reference_from_location(iter->second);
                            updates.emplace_back(move(ref_ref), move(new_ref));
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
    catch (...) { reload_rollback(reses, move(old_values)); throw; }
     // Commit step
    if (ResourceTransaction::depth) {
        set_states(reses, RS::ReloadReady);
        struct ReloadCommitter : Committer {
            UniqueArray<SharedResource> rs;
            UniqueArray<Update> updates;
            UniqueArray<Dynamic> old_values;
            ReloadCommitter (
                UniqueArray<SharedResource>&& r,
                UniqueArray<Update>&& u,
                UniqueArray<Dynamic>&& o
            ) : rs(move(r)), updates(move(u)), old_values(move(o))
            { }
            void commit () noexcept override {
                reload_commit(rs.reinterpret<ResourceRef>(), move(updates));
            }
            void rollback () noexcept override {
                reload_rollback(
                    rs.reinterpret<ResourceRef>(), move(old_values)
                );
            }
        };
        ResourceTransaction::add_committer(
            new ReloadCommitter(
                UniqueArray<SharedResource>(reses),
                move(updates), move(old_values)
            )
        );
    }
    else reload_commit(reses, move(updates));
}

void rename (ResourceRef old_res, ResourceRef new_res) {
    auto old_data = static_cast<ResourceData*>(old_res.data);
    auto new_data = static_cast<ResourceData*>(new_res.data);
    if (old_data->state != RS::Loaded) {
        raise_ResourceStateInvalid("rename from", old_res);
    }
    if (new_data->state != RS::Unloaded) {
        raise_ResourceStateInvalid("rename to", new_res);
    }
    expect(!new_data->value.has_value());
    new_data->value = move(old_data->value);
    new_data->state = RS::Loaded;
    old_data->state = RS::Unloaded;
}

AnyString resource_filename (const IRI& name) {
    auto scheme = universe().require_scheme(name);
    return scheme->get_file(name);
}

void remove_source (const IRI& name) {
    auto scheme = universe().require_scheme(name);
    auto filename = scheme->get_file(name);
    remove_utf8(filename.c_str());
}

bool source_exists (const IRI& name) {
    auto scheme = universe().require_scheme(name);
    auto filename = scheme->get_file(name);
    if (std::FILE* f = fopen_utf8(filename.c_str())) {
        fclose(f);
        return true;
    }
    else return false;
}

UniqueArray<SharedResource> loaded_resources () noexcept {
    UniqueArray<SharedResource> r;
    for (auto& [name, rd] : universe().resources)
    if (rd->state() != RS::Unloaded) {
        r.push_back(rd);
    }
    return r;
}

} using namespace ayu;

///// DESCRIPTIONS

AYU_DESCRIBE(ayu::SharedResource,
    delegate(const_ref_funcs<IRI>(
        [](const SharedResource& v) -> const IRI& {
            return v->name();
        },
        [](SharedResource& v, const IRI& m){
            v = SharedResource(m);
        }
    ))
)
AYU_DESCRIBE(ayu::ResourceRef,
    delegate(const_ref_func<IRI>(
        [](const ResourceRef& v) -> const IRI& {
            return v->name();
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

    SharedResource input ("ayu-test:/testfile.ayu");
    SharedResource input2 ("ayu-test:/othertest.ayu");
    SharedResource rec1 ("ayu-test:/rec1.ayu");
    SharedResource rec2 ("ayu-test:/rec2.ayu");
    SharedResource badinput ("ayu-test:/badref.ayu");
    SharedResource output ("ayu-test:/test-output.ayu");
    SharedResource unicode ("ayu-test:/ユニコード.ayu");
    SharedResource unicode2 ("ayu-test:/ユニコード2.ayu");

    is(input->state(), RS::Unloaded, "Resources start out unloaded");
    doesnt_throw([&]{ load(input); }, "load");
    is(input->state(), RS::Loaded, "Resource state is RS::Loaded after loading");
    ok(input->value().has_value(), "Resource has value after loading");

    throws_code<e_ResourceStateInvalid>([&]{
        SharedResource(input->name(), Dynamic::make<int>(3));
    }, "Creating resource throws on duplicate");

    doesnt_throw([&]{ unload(input); }, "unload");
    is(input->state(), RS::Unloaded, "Resource state is RS::Unloaded after unloading");
    ok(!input->get_value().has_value(), "Resource has no value after unloading");

    ayu::Document* doc = null;
    doesnt_throw([&]{
        doc = &input->value().as<ayu::Document>();
    }, "Getting typed value from a resource");
    is(input->state(), RS::Loaded, "Resource::value() automatically loads resource");
    is(input["foo"][1].get_as<int32>(), 4, "Value was generated properly (0)");
    is(input["bar"][1].get_as<std::string>(), "qux", "Value was generated properly (1)");

    throws_code<e_ResourceStateInvalid>([&]{ save(output); }, "save throws on unloaded resource");

    doc->delete_named("foo");
    doc->new_named<int32>("asdf", 51);

    doesnt_throw([&]{ rename(input, output); }, "rename");
    is(input->state(), RS::Unloaded, "Old resource is RS::Unloaded after renaming");
    is(output->state(), RS::Loaded, "New resource is RS::Loaded after renaming");
    is(&output->value().as<ayu::Document>(), doc, "Rename moves value without reconstructing it");

    doesnt_throw([&]{ save(output); }, "save");
    is(tree_from_file(resource_filename(output->name())), tree_from_string(
        "[ayu::Document {bar:[std::string qux] asdf:[int32 51] _next_id:0}]"
    ), "Resource was saved with correct contents");
    ok(source_exists(output->name()), "source_exists returns true before deletion");
    doesnt_throw([&]{ remove_source(output->name()); }, "remove_source");
    ok(!source_exists(output->name()), "source_exists returns false after deletion");
    throws_code<e_OpenFailed>([&]{
        tree_from_file(resource_filename(output->name()));
    }, "Can't open file after calling remove_source");
    doesnt_throw([&]{ remove_source(output->name()); }, "Can call remove_source twice");
    SharedLocation loc;
    doesnt_throw([&]{
        item_from_string(&loc, cat('"', input->name().spec(), "#/bar+1\""));
    }, "Can read location from tree");
    Reference ref;
    doesnt_throw([&]{
        ref = reference_from_location(loc);
    }, "reference_from_location");
    doesnt_throw([&]{
        is(ref.get_as<std::string>(), "qux", "reference_from_location got correct item");
    });
    doc = &output->value().as<ayu::Document>();
    ref = output["asdf"][1].address_as<int32>();
    doesnt_throw([&]{
        loc = reference_to_location(ref);
    });
    is(item_to_tree(&loc), tree_from_string("\"ayu-test:/test-output.ayu#/asdf+1\""), "reference_to_location works");
    doc->new_<Reference>(output["bar"][1]);
    doesnt_throw([&]{ save(output); }, "save with reference");
    doc->new_<int32*>(output["asdf"][1]);
    doesnt_throw([&]{ save(output); }, "save with pointer");
    is(tree_from_file(resource_filename(output->name())), tree_from_string(
        "[ayu::Document {bar:[std::string qux] asdf:[int32 51] _0:[ayu::Reference #/bar+1] _1:[int32* #/asdf+1] _next_id:2}]"
    ), "File was saved with correct reference as location");
    throws_code<e_OpenFailed>([&]{
        load(badinput);
    }, "Can't load file with incorrect reference in it");

    doesnt_throw([&]{
        unload(input);
        load(input2);
    }, "Can load second file referencing first");
    is(input->state(), RS::Loaded, "Loading second file referencing first file loads first file");
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
        load(SharedResource("ayu-test:/wrongtype.ayu"));
    }, "ResourceScheme::accepts_type rejects wrong type");

    done_testing();
});
#endif
