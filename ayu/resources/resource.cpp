#include "resource.h"

#include "../../iri/iri.h"
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
#include "scheme.h"
#include "universe.private.h"

///// INTERNALS

namespace ayu {
namespace in {

struct ResourceData : Resource {
    ResourceState state = RS::Unloaded;
     // These are only used during reachability scanning, but we have extra room
     // for them here.
    bool root;
    bool reachable;
     // This is also only used during reachability scanning, and we don't have
     // extra room for it, but storing it externally would require using an
     // unordered_map (to use a UniqueArray, we need an integer index, but
     // that's what this itself is).
    uint32 node_id;
    IRI name;
    Dynamic value {};
    ResourceData (const IRI& n) : name(n) { }
};

[[noreturn, gnu::cold]]
static void raise_ResourceStateInvalid (StaticString tried, ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    raise(e_ResourceStateInvalid, cat(
        "Can't ", tried, ' ', data->name.spec(),
        " when its state is ", item_to_string(&data->state)
    ));
}

[[noreturn, gnu::cold]]
static void raise_ResourceValueEmpty (StaticString tried, ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    raise(e_ResourceValueInvalid, cat(
        "Can't ", tried, ' ', data->name.spec(), " with empty value"
    ));
}

[[noreturn, gnu::cold]]
static void raise_ResourceTypeRejected (
    StaticString tried, ResourceRef res, Type type
) {
    auto data = static_cast<ResourceData*>(res.data);
    raise(e_ResourceTypeRejected, cat(
        "Can't ", tried, ' ', data->name.spec(), " with type ", type.name()
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

struct ROV {
     // Since this keeps a ref count on the ResourceData, if unload is called
     // with a ResourceData that has a ref count of 0 (but wasn't deleted
     // because it was loaded), then when this object is destroyed the ref count
     // will go back to 0 and the ResourceData will be actually deleted (unless
     // it was rolled back).
    SharedResource res;
    Dynamic old_value;
    void rollback () {
        auto data = static_cast<ResourceData*>(res.data.p);
        data->value = move(old_value);
        data->state = RS::Loaded;
    }
};

} using namespace in;

///// ACCESSORS

const IRI& Resource::name () const noexcept {
    return static_cast<const ResourceData*>(this)->name;
}
ResourceState Resource::state () const noexcept {
    return static_cast<const ResourceData*>(this)->state;
}

Dynamic& Resource::value () {
    auto data = static_cast<ResourceData*>(this);
    if (data->state == RS::Unloaded) {
        load(ResourceRef(this));
    }
    return data->value;
}
Dynamic& Resource::get_value () noexcept {
    return static_cast<ResourceData*>(this)->value;
}

void Resource::set_value (Dynamic&& value) {
    auto data = static_cast<ResourceData*>(this);
    Dynamic v = move(value);
    if (data->state == RS::Loading) {
        raise_ResourceStateInvalid("set_value", this);
    }
    if (!v) {
        raise_ResourceValueEmpty("set_value", this);
    }
    if (data->name) {
        auto scheme = universe().require_scheme(data->name);
        if (!scheme->accepts_type(v.type)) {
            raise_ResourceTypeRejected("set_value", this, v.type);
        }
    }
    if (ResourceTransaction::depth) {
        struct SetValueCommitter : Committer {
            SharedResource res;
            Dynamic old_value;
            SetValueCommitter (SharedResource&& r, Dynamic&& v) :
                res(move(r)), old_value(move(v))
            { }
            void rollback () noexcept override {
                auto data = static_cast<ResourceData*>(res.data.p);
                data->value = move(old_value);
                data->state = data->value ?  RS::Loaded : RS::Unloaded;
            }
        };
        ResourceTransaction::add_committer(
            new SetValueCommitter(this, move(data->value))
        );
    }
    data->value = move(v);
    data->state = RS::Loaded;
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

SharedResource::SharedResource (const IRI& name, Dynamic&& value) :
    SharedResource(name)
{
    Dynamic v = move(value);
    if (!v) {
        raise_ResourceValueEmpty("construct", *this);
    }
    else if (data->state() == RS::Unloaded) data->set_value(move(v));
    else raise_ResourceStateInvalid("construct", *this);
}

void in::delete_Resource_if_unloaded (Resource* res) noexcept {
    auto data = static_cast<ResourceData*>(res);
    if (data->state == RS::Unloaded) {
        universe().resources.erase(data->name.spec());
        delete data;
    }
}

///// RESOURCE OPERATIONS

static void load_cancel (ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    data->value = {};
    data->state = RS::Unloaded;
}

void load (ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    if (data->state != RS::Unloaded) return;

    data->state = RS::Loading;
    try {
        auto scheme = universe().require_scheme(data->name);
        auto filename = scheme->get_file(data->name);
        Tree tree = tree_from_file(move(filename));
        auto tnt = verify_tree_for_scheme(res, scheme, tree);
         // Run item_from_tree on the Dynamic's value, not on the Dynamic
         // itself.  Otherwise, the associated locations will have an extra +1
         // in the fragment.
        expect(!data->value);
        data->value = Dynamic(tnt.type);
        item_from_tree(
            data->value.ptr(), tnt.tree, SharedLocation(res),
            FromTreeOptions::DelaySwizzle
        );
    }
    catch (...) { load_cancel(res); throw; }

    if (ResourceTransaction::depth) {
        struct LoadCommitter : Committer {
            SharedResource res;
            LoadCommitter (SharedResource&& r) : res(move(r)) { }
            void rollback () noexcept override {
                load_cancel(res);
            }
        };
        ResourceTransaction::add_committer(
            new LoadCommitter(res)
        );
    }
    data->state = RS::Loaded;
}

void save (ResourceRef res, PrintOptions opts) {
    if (!(opts & PrintOptions::Compact)) opts |= PrintOptions::Pretty;
    auto data = static_cast<ResourceData*>(res.data);
    if (data->state != RS::Loaded) {
        raise_ResourceStateInvalid("save", res);
    }

    KeepLocationCache klc;
    if (!data->value) {
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
        Tree::array(move(type_tree), move(value_tree)), opts
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

static void really_unload (ResourceData* data) {
    if (ResourceTransaction::depth) {
        struct ForceUnloadCommitter : Committer {
            ROV rov;
            ForceUnloadCommitter (ROV&& r) : rov(move(r)) { }
            void rollback () noexcept override {
                rov.rollback();
            }
        };
        ResourceTransaction::add_committer(
            new ForceUnloadCommitter({data, move(data->value)})
        );
        data->state = RS::Unloaded;
    }
    else {
        data->value = {};
        if (!data->ref_count) {
            universe().resources.erase(data->name.spec());
            delete data;
        }
        else data->state = RS::Unloaded;
    }
}

struct ResourceScanInfo {
    ResourceData* data;
    UniqueArray<Reference> outgoing_refs;
};
using RefsToReses = std::unordered_map<Reference, ResourceData*>;

static void reach_reference (
    const UniqueArray<ResourceScanInfo>& scan_info,
    const RefsToReses& refs_to_reses,
    const Reference& item
) {
    auto it = refs_to_reses.find(item);
    if (it == refs_to_reses.end()) {
         // Reference is already invalid?  Either that or it points to the
         // root set, which we didn't bother studying because we already
         // know it's reachable.
        return;
    }
    auto to_data = it->second;
    if (to_data->reachable) return;
    to_data->reachable = true;
    for (auto& ref : scan_info[to_data->node_id].outgoing_refs) {
        reach_reference(scan_info, refs_to_reses, ref);
    }
}

void unload (Slice<ResourceRef> to_unload) {
    auto& resources = universe().resources;
     // TODO: Track how many loaded resources there are to preallocate this.
    auto scan_info = UniqueArray<ResourceScanInfo>(Capacity(resources.size()));
     // Start out be getting a bit of info about all loaded resources.
    bool none_root = true;
    bool all_root = true;
    for (auto& [name, res] : resources) {
        auto data = static_cast<ResourceData*>(res.data);
         // Only scan loaded resources
        if (data->state != RS::Loaded) continue;
         // Assign integer ID for indexing
        data->node_id = scan_info.size();
        scan_info.emplace_back_expect_capacity(data, UniqueArray<Reference>());
         // Our root set for the reachability traversal is all resources that
         // have a reference count but were not explicitly requested to be
         // unloaded.
        if (data->ref_count) {
            data->root = true;
            for (auto& tu : to_unload) {
                if (tu == res) {
                    data->root = false;
                    break;
                }
            }
        }
        else data->root = false;
        if (data->root) none_root = false;
        else all_root = false;
        data->reachable = false;
    }
    if (all_root) {
         // All resources are still in use and no resources were requested to be
         // unloaded.  Everyone can go home.
        return;
    }
    if (none_root && !universe().globals) {
         // Root set is empty!  We get to skip reachability scanning and just
         // unload everything.
        scan_info.consume([](auto&& info){ really_unload(info.data); });
        return;
    }
     // Collect as much info as we can from one scan.  Unfortunately we can't
     // traverse the data graph directly, because finding out what Resource a
     // Reference points to requires a full scan itself.  We don't have to cache
     // as much data as reference_to_location though; we only need to keep track
     // of the Location's root, not the whole Location itself.
    auto refs_to_reses = std::unordered_map<Reference, ResourceData*>();
    for (auto& info : scan_info) {
         // TODO: Don't generate locations if we're throwing them away
        scan_resource_references(info.data,
            [&refs_to_reses, &info](const Reference& item, LocationRef)
        {
             // Don't need to enumerate references for resources in the root
             // set, because they start out reachable.
            if (!info.data->root) {
                refs_to_reses.emplace(item, info.data);
            }
            if (item.type() == Type::CppType<Reference>()) {
                if (auto ref = item.get_as<Reference>()) {
                    info.outgoing_refs.emplace_back(move(ref));
                }
            }
            return false;
        });
    }
     // Now traverse the graph starting with the globals and roots.
    for (auto& g : universe().globals) {
        scan_references(
            g, {},
            [&scan_info, refs_to_reses](const Reference& item, LocationRef)
        {
            if (item.type() == Type::CppType<Reference>()) {
                reach_reference(scan_info, refs_to_reses, item);
            }
            return false;
        });
    }
    for (auto& info : scan_info) {
        if (info.data->root) {
            info.data->reachable = true;
            for (auto& ref : scan_info[info.data->node_id].outgoing_refs) {
                reach_reference(scan_info, refs_to_reses, ref);
            }
        }
    }
     // At this point, all resources should be marked whether they're reachable.
     // First throw an error if any resources we were explicitly told to unload
     // are still reachable.
    for (auto res : to_unload) {
        auto data = static_cast<ResourceData*>(res.data);
        if (data->reachable) {
            raise(e_ResourceUnloadWouldBreak, cat(
                "Cannot unload resource ", data->name.spec(),
                " because it is still reachable.  Further info NYI."
            ));
        }
    }
     // Now finally unload all unreachable resources.
    for (auto& info : scan_info) {
        if (!info.data->reachable) really_unload(info.data);
    }
}

void force_unload (ResourceRef res) noexcept {
    auto data = static_cast<ResourceData*>(res.data);
    switch (data->state) {
        case RS::Unloaded: return;
        case RS::Loaded: break;
        default: raise_ResourceStateInvalid("force_unload", res);
    }
    really_unload(data);
}

struct Update {
    Reference ref_ref;
    Reference new_ref;
};

NOINLINE static void reload_commit (MoveRef<UniqueArray<Update>> ups) {
    auto updates = *move(ups);
    updates.consume([](Update&& update){
        if (auto a = update.ref_ref.address()) {
            reinterpret_cast<Reference&>(*a) = move(update.new_ref);
        }
        else update.ref_ref.write([new_ref{&update.new_ref} ](Mu& v){
            reinterpret_cast<Reference&>(v) = move(*new_ref);
        });
    });
}

NOINLINE static void reload_rollback (MoveRef<UniqueArray<ROV>> rs) {
    auto rovs = *move(rs);
    rovs.consume([](auto&& rov){
        auto data = static_cast<ResourceData*>(rov.res.data.p);
        data->value = move(rov.old_value);
    });
}

void reload (Slice<ResourceRef> reses) {
    UniqueArray<ROV> rovs;
    for (auto res : reses) {
        if (res->state() == RS::Loaded) {
            rovs.push_back({res, {}});
        }
        else raise_ResourceStateInvalid("reload", res);
    }
     // Preserve step
    for (auto& rov : rovs) {
        auto data = static_cast<ResourceData*>(rov.res.data.p);
        rov.old_value = move(data->value);
    }

    UniqueArray<Update> updates;
    try {
         // Construct step
         // TODO: Start ResourceTransaction for dependently-loaded resources.
        for (auto res : reses) {
            auto data = static_cast<ResourceData*>(res.data);
            data->state = RS::Loading;
            auto scheme = universe().require_scheme(data->name);
            auto filename = scheme->get_file(data->name);
            Tree tree = tree_from_file(move(filename));
            auto tnt = verify_tree_for_scheme(res, scheme, tree);
            expect(!data->value);
            data->value = Dynamic(tnt.type);
             // Do not DelaySwizzle for reload.  TODO: Forbid reload while a
             // serialization operation is ongoing.
            item_from_tree(data->value.ptr(), tnt.tree, SharedLocation(res));
            data->state = RS::Loaded;
        }
         // Verify step
        UniqueArray<ResourceRef> others;
        for (auto& [name, other] : universe().resources) {
            switch (other->state()) {
                case RS::Unloaded: continue;
                case RS::Loaded: others.emplace_back(&*other); break;
                default: raise_ResourceStateInvalid("scan for reload", &*other);
            }
            for (auto res : reses) {
                if (res == other) goto next_other;
            }
            next_other:;
        }
         // If we're reloading everything, no need to do any scanning.
        if (others) {
             // First build mapping of old refs to locations
            std::unordered_map<Reference, SharedLocation> old_refs;
            for (auto& rov : rovs) {
                scan_references(
                    rov.old_value.ptr(), SharedLocation(rov.res),
                    [&old_refs](const Reference& ref, LocationRef loc) {
                        old_refs.emplace(ref, loc);
                        return false;
                    }
                );
            }
             // Then build set of ref-refs to update.
            UniqueArray<Break> breaks;
            auto check_ref =
                [&updates, &old_refs, &breaks](Reference ref_ref, LocationRef loc)
            {
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
            };
            for (auto global : universe().globals) {
                scan_references(global, {}, check_ref);
            }
            for (auto other : others) {
                scan_resource_references(other, check_ref);
            }
            if (breaks) {
                raise_would_break(e_ResourceReloadWouldBreak, move(breaks));
            }
        }
    }
    catch (...) { reload_rollback(move(rovs)); throw; }
     // Commit step.  TODO: Update references now and roll them back if
     // necessary
    if (ResourceTransaction::depth) {
        struct ReloadCommitter : Committer {
            UniqueArray<ROV> rovs;
            UniqueArray<Update> updates;
            ReloadCommitter (UniqueArray<ROV>&& r, UniqueArray<Update>&& u) :
                rovs(move(r)), updates(move(u))
            { }
            void commit () noexcept override {
                reload_commit(move(updates));
            }
            void rollback () noexcept override {
                reload_rollback(move(rovs));
            }
        };
        ResourceTransaction::add_committer(
            new ReloadCommitter(move(rovs), move(updates))
        );
    }
    else reload_commit(move(updates));
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
    expect(!new_data->value);
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

AYU_DESCRIBE(ayu::ResourceState,
    values(
        value("unloaded", RS::Unloaded),
        value("loading", RS::Loading),
        value("loaded", RS::Loaded)
    )
)
AYU_DESCRIBE(ayu::SharedResource,
    delegate(const_ref_funcs<IRI>(
        [](const SharedResource& v) -> const IRI& {
             // TODO: Make relative to current resource?
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
#include "global.h"

AYU_DESCRIBE_INSTANTIATE(std::vector<int32*>)

static tap::TestSet tests ("dirt/ayu/resources/resource", []{
    using namespace tap;

    test::TestEnvironment env;

    SharedResource input (IRI("ayu-test:/testfile.ayu"));
    SharedResource input2 (IRI("ayu-test:/othertest.ayu"));
    SharedResource rec1 (IRI("ayu-test:/rec1.ayu"));
    SharedResource rec2 (IRI("ayu-test:/rec2.ayu"));
    SharedResource badinput (IRI("ayu-test:/badref.ayu"));
    SharedResource output (IRI("ayu-test:/test-output.ayu"));
    SharedResource unicode (IRI("ayu-test:/ユニコード.ayu"));
    SharedResource unicode2 (IRI("ayu-test:/ユニコード2.ayu"));

    is(input->state(), RS::Unloaded, "Resources start out unloaded");
    doesnt_throw([&]{ load(input); }, "load");
    is(input->state(), RS::Loaded, "Resource state is RS::Loaded after loading");
    ok(!!input->value(), "Resource has value after loading");

    throws_code<e_ResourceStateInvalid>([&]{
        SharedResource(input->name(), Dynamic::make<int>(3));
    }, "Creating resource throws on duplicate");

    doesnt_throw([&]{ unload(input); }, "unload");
    is(input->state(), RS::Unloaded, "Resource state is RS::Unloaded after unloading");
    ok(!input->get_value(), "Resource has no value after unloading");

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
     // TODO: test that calling unload unloads dependent resources if there are
     // no SharedResource handles pointing to them.

    load(rec1);
    int* old_p = rec1["ref"][1].get_as<int*>();
    int* global_p = old_p;
    ayu::global(&global_p);

    doesnt_throw([&]{
        reload(rec2);
    }, "Can reload file with references to it");
    int* new_p = rec1["ref"][1].get_as<int*>();
    isnt(new_p, old_p, "Reference to reloaded file was updated");
    is(global_p, new_p, "Global was updated.");

    throws_code<e_ResourceTypeRejected>([&]{
        load(SharedResource(IRI("ayu-test:/wrongtype.ayu")));
    }, "ResourceScheme::accepts_type rejects wrong type");

    done_testing();
});
#endif
